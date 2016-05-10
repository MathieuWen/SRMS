#include <stdlib.h>
#include <math.h>
#include "rtp.h"
#include "srms-sink.h"
#include <map>
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

//string representation of types
char* SrmSinkAgent::state_str_[SRMS_NUM_STATES] =
{"INITIAL_ABWE_STATE", "EXTRA_ABWE_STATE", "NORMAL_STATE", "PENDING_STATE", "CONGESTION_AVOIDANCE_STATE"};
char* SrmSinkAgent::ptype_str_[SRMS_NUM_PTYPES] =
{"SRMS_PROBE","SRMS_IPROBE", "SRMS_DATA", "SRMS_PROBEACK", "SRMS_IPROBEACK", "SRMS_DATACK", "SRMS_LOSS"};


static class SRMSinkClass : public TclClass {
public:
    SRMSinkClass() : TclClass("Agent/UDP/SRMSink") {}
    TclObject* create(int, const char*const*) {
        return (new SrmSinkAgent());
    }
} class_srmsink_agent;
 

SrmSinkAgent::SrmSinkAgent() : UdpAgent(),
timer_qs_(this)
{
    init();
}

SrmSinkAgent::SrmSinkAgent(packet_t type) : UdpAgent(type),
timer_qs_(this)
{
    init();
}


void SrmSinkAgent::init()
{
    bytes_ = 0;
    nlost_ = 0;
    npkts_ = 0;
    expected_ = -1;
    state_ = PENDING_STATE;
    old_state_ = PENDING_STATE;
    last_packet_time_ = 0.0;
    seqno_ = 0;
	preseqno_ = 0;
	maxseqno_ = -1;
    prets_ = 0;
    sum_ipdv_ = 0;
    sum_owd_ = 0;
    current_owd_ = 0;     
    current_ipdv_ = 0;
    previous_owd_ = 0;
    min_owd_ = 100000000;
	max_owd_ = -1;
    ndis_ = 100;
	min_rate_interval_ = 0.008;
	rowd_ = 0.1;
    pdis_ = 0;
	rate_interval_ = 0;
    qsMatrixLeng = 0;
	owdMatrixLeng = 0;
	sowd_ = 0;
	owdvar_ = 0;
	owd_alpha_ = 1/8;
	owd_beta_ = 1/4;
	lossthreshold_ = 0.03;
	owd_K_ = 4;
	FS_ = 0;
	QFS_ = 0;
	P_ = 0;
	session_ = 0;
    loss_rate_ = 0.0;
	total_pktsizeno_ = 0;
	total_dispersion_ = 0;
	s_dispersion_ = 0;
	probing_pkt_no_ = 0;
	data_pkt_no_ = 0;
	flagTvN_ = false;
	loss_has_been_alert = false;
    qsMatrix = (int*)calloc((int)ndis_, (int)sizeof(int));
    dispersion = (double*)calloc((int)ndis_, (int)sizeof(double));
    owd = (double*)calloc((int)ndis_, (int)sizeof(double));
	
    min_owd_group_ = 9999;
    max_owd_group_ = 0;
    avg_owd_group_ = 0;
	
    static unsigned int s_agent_ID = 0;
    char strTmp[30];
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "owd_flow");
	pFile = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "timeout");
    pFile2 = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "income");
    pFile3 = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "dispersion");
    pFile4 = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "qsize");
    pFile5 = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "qsize2");
    pFile6 = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "qsizeFS");
    pFile7 = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "loss");
    pFile8 = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "owdFS");
    pFile9 = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "owdtrend");
    pFile10 = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "states");
    pFile11 = fopen(strTmp, "w");
    s_agent_ID++;
}

/* OTcl binding of variables */
void SrmSinkAgent::delay_bind_init_all(){
	delay_bind_init_one("packetSize_");
	delay_bind_init_one("nlost_");
	delay_bind_init_one("npkts_");
	delay_bind_init_one("bytes_");
	delay_bind_init_one("snd_interval_");
	delay_bind_init_one("interval_");
	delay_bind_init_one("expected_");
	delay_bind_init_one("probetrain_");
	delay_bind_init_one("datatrain_");
	delay_bind_init_one("trainSize_");
	delay_bind_init_one("flagTvN_");
	
	Agent::delay_bind_init_all();
}

int SrmSinkAgent::delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer){
	if (delay_bind(varName, localName, "packetSize_", &size_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "nlost_", &nlost_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "npkts_", &npkts_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "bytes_", &bytes_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "snd_interval_", &snd_interval_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "interval_", &interval_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "expected_", &expected_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "trainSize_", &trainSize_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "probetrain_", &probetrain_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "datatrain_", &datatrain_ , tracer)) return TCL_OK;
	if (delay_bind_bool(varName, localName, "flagTvN_", &flagTvN_ , tracer)) return TCL_OK;
	return Agent::delay_bind_dispatch(varName, localName, tracer);
}

void SrmSinkAgent::owdtrend(double now__)
{
	if(owdMatrixLeng>1){
		int identity = 0, M = 0;
		double FS = 0.0;
        double sum_owd__ = 0;
		for(int i=0; i<owdMatrixLeng; i++){
			for(int l=0; l<i; l++){
				if(owd[i]>owd[l]){
					++identity;
				}
			}
			//fprintf(pFile9, "%0.4f %0.4f\n", now__ , owd[i]);
            
            sum_owd__ += owd[i];
		}
		
        avg_owd_group_ = sum_owd__ / owdMatrixLeng;
        
        if(avg_owd_group_>max_owd_group_)
            max_owd_group_ = avg_owd_group_;
        if(avg_owd_group_<min_owd_group_)
            min_owd_group_ = avg_owd_group_;

		//double d = max_owd_group_-min_owd_group_;
		double d = max_owd_-min_owd_;
		if(d>0){
			//P_ = (avg_owd_group_-min_owd_group_)/d;
			P_ = (avg_owd_group_-min_owd_)/d;
		}else{
			P_ = 0;
		}
        
		M = (owdMatrixLeng*(owdMatrixLeng-1))/2;
		FS = double(identity)/double(M);
		FS_ = FS;
		fprintf(pFile9, "%0.4f %d %d %0.4f %0.6f\n", now__ , identity, M, FS, P_);
	}else
		fprintf(stderr,"%f, SRMSink(%s)::owdtrend() - owdMatrixLeng <= 1!\n", now(), name());

}

void SrmSinkAgent::qstrend(double now__)
{
    if(qsMatrixLeng>1){
		int identity = 0, M = 0;
		double QFS = 0.0f;
		int qsMax = -1, qsMin = 999999;
		int qsDiff = 0;
		double PDT = 0.0, PDT2 = 0.0;
		for(int i=0; i<qsMatrixLeng; i++){
			for(int l=0; l<i; l++){
				if(qsMatrix[i]>qsMatrix[l]){
					++identity;
				}
			}
			//fprintf(pFile7, "%0.4f %d\n", now__ , qsMatrix[i]);
			if(qsMatrix[i]>qsMax){
				qsMax = qsMatrix[i];
			}
			if(qsMatrix[i]<qsMin){
				qsMin = qsMatrix[i];
			}
			if(i>=1){
				qsDiff += abs(qsMatrix[i] - qsMatrix[i-1]);
			}
		}
		
		M = (qsMatrixLeng*(qsMatrixLeng-1))/2;
		if(M==0)
			QFS = 0.0f;
		else
			QFS = double(identity)/double(M);
		
		QFS_ = QFS;
		if(qsDiff){
			PDT = double(qsMax-qsMin)/double(qsDiff);
			PDT2 = double(qsMatrix[qsMatrixLeng-1]-qsMatrix[0])/double(qsDiff);
		}
		
		fprintf(pFile7, "%0.4f %d %d %d %0.4f %d %d %d %0.4f %0.4f\n", now__ , qsMatrixLeng, identity, M, QFS, qsMax, qsMin, qsDiff, PDT, PDT2);
        qsMatrixLeng = 0;
	}else
		fprintf(stderr,"%f, SRMSink(%s)::qstrend() - qsMatrixLeng <= 1!\n", now(), name());
}

int SrmSinkAgent::round(double x)
{
    double base_ = 0.50;
    double round__ = 0;
    if(x>=0.0){
        if(floor(x+base_)==floor(x)){
            round__ = floor(x);
        }else if(floor(x+base_)>floor(x)){
            round__ = ceil(x);
        }else{
            fprintf(stderr,"%f, SRMS(%s)::round() - arithmetic error!\n", now(), name());
        }
    }
    else{
        if(floor(-x+base_)==floor(-x)){
            round__ = -floor(x);
        }else if(floor(-x+base_)>floor(-x)){
            round__ = -ceil(x);
        }else{
            fprintf(stderr,"%f, SRMS(%s)::round() - arithmetic error!\n", now(), name());
        }
    }
    return int(round__);
}

bool SrmSinkAgent::fequal(double a, double b)
{
	return fabs(a-b) < EPSILON;
}

void SrmSinkAgent::estimationQSize(double now__, int seqno__, int pdis, int qsztrain__, double dispersion__)
{
    int ptrdis = pdis;
	if(npkts_>=ndis_){
		ptrdis = (ptrdis+ndis_-1) % ndis_;
	}else{
		ptrdis = ptrdis - 1;
	}
	
    //double qdelay = current_owd - min_owd - dispersion[pdis];
    //double qdelay = current_owd_ - min_owd_ - dispersion__;
    double qdelay = current_owd_ - min_owd_;
    int qsize = 0;
    while(qdelay > EPSILON){
		if(dispersion[ptrdis] > 100.0){
			qsize += round(qdelay/s_dispersion_);
			//debug("%0.4f, SrmSinkAgent(%s)::estimationQSize() - qsize is %d\n", now(), name(), qsize);
			//fprintf(pFile5, "%0.4f %d %d\n", now__ , ptrdis, round(qdelay/s_dispersion_));
			break;
		}
		else{
			qdelay -= dispersion[ptrdis];
			if(npkts_>=ndis_){
				ptrdis = (ptrdis+ndis_-1) % ndis_;
			} else {
				ptrdis = ptrdis - 1;
			} 
			++qsize;
		}
    }
    
	//if(qsMatrixLeng < qsztrain__ && qsMatrixLeng>=0){
	if( qsMatrixLeng>=0 ){
		qsMatrix[qsMatrixLeng] = qsize;
		qsMatrixLeng++;
	}
    else{
		fprintf(stderr,"%f, SRMSink(%s)::estimationQSize() - qsMatrixLeng(%d) < 0 OR qsMatrixLeng > qsztrain__(%d) !\n", now(), name(), qsMatrixLeng, qsztrain__);
	}
    
    fprintf(pFile5, "%0.6f,%d,%d,%0.6f,%0.6f,%0.6f,%0.6f,%d\n", now__, seqno__, pdis, current_owd_, min_owd_, dispersion__, current_owd_ - min_owd_ - dispersion__, qsize);
}

void SrmSinkAgent::estimationOWD(double now__, int seqno__, double ts__, int owdtrain__)
{
	if(now__-ts__>=0){
		current_owd_ = now__-ts__;
		if(sowd_> 0.0f){
			owdvar_ = (1-owd_beta_)*owdvar_+owd_beta_*abs(sowd_-current_owd_);
			sowd_ = (1-owd_alpha_)*sowd_+owd_alpha_*current_owd_;
		}
		else{
			owdvar_ = current_owd_/2;
			sowd_ = current_owd_;
		}
		
		//rowd_ = sowd_ + MAX(0.1, owd_K_*owdvar_);
		
		
		//看看量測到的owd是否是最小值
		if (current_owd_<min_owd_)
			min_owd_ = current_owd_;
		if (current_owd_>max_owd_)
			max_owd_ = current_owd_;
		
		rowd_ = max_owd_ * 3;
		
		if(owdMatrixLeng < owdtrain__ && owdMatrixLeng>=0){
			owd[owdMatrixLeng] = current_owd_;
			owdMatrixLeng++;
		}else{
			fprintf(stderr,"%f, SRMSink(%s)::estimationOWD() - owdMatrixLeng(%d) < 0 OR owdMatrixLeng > owdtrain__(%d) !\n", now(), name(), owdMatrixLeng, owdtrain__);
		}
        
        fprintf(pFile, "%0.4f %d %0.6f %0.6f %0.6f %0.6f\n", now__, seqno__, min_owd_, max_owd_, sowd_, current_owd_);
	}else{
		fprintf(stderr,"%f, SRMSink(%s)::estimationOWD() - owd < 0!\n", now(), name());
	}
	
}


void SrmSinkAgent::recv(Packet* pkt, Handler*)
{
	hdr_srms *sh = hdr_srms::access(pkt);
	hdr_rtp *rh = hdr_rtp::access(pkt);
    seqno_ = rh->seqno();
    session_ = sh->session_;
	rate_interval_ = sh->rateInterval();
	snd_rate_interval_ = sh->rateInterval();
	++npkts_;
    now_ = now();
	bytes_ += hdr_cmn::access(pkt)->size();
	
    if(maxseqno_<seqno_)
		maxseqno_ = seqno_;

	int loss = 0;
    //檢查封包是否有移失
    if (expected_ >= 0) {
        //從封包序號的不連續性,就可以得知是否有封包移失
        loss = seqno_ - expected_;
        
        if (loss > 0) {
            nlost_ += loss;
            //Tcl::instance().evalf("%s log-loss", name());
            fprintf(pFile8, "%0.4f %d %s %d %d\n", now_, seqno_, packetTypeAsStr(sh->type()), nlost_, loss);
        }
    }
	
	switch(sh->type()){
		case SRMS_IPROBE:
            {
				if(state_!=INITIAL_ABWE_STATE){
					qsMatrixLeng = 0;
					owdMatrixLeng = 0;
					probing_pkt_no_ = 0;
				}
					
                changeState(INITIAL_ABWE_STATE);
                ++probing_pkt_no_;
            }
		break;
		case SRMS_PROBE:
            {
				if(state_!=EXTRA_ABWE_STATE){
					qsMatrixLeng = 0;
					owdMatrixLeng = 0;
					probing_pkt_no_ = 0;
				}
                changeState(EXTRA_ABWE_STATE);
                ++probing_pkt_no_;
            }
		break;
		case SRMS_DATA:
            {
				if(state_!=NORMAL_STATE){
					qsMatrixLeng = 0;
					owdMatrixLeng = 0;
					data_pkt_no_ = 0;
				}
                changeState(NORMAL_STATE);
                /*
				if(sh->seqno()==0){
					nlost_ = 0;
				}
				else if(data_pkt_no_==0){
					if(sh->seqno()>0){
						nlost_ = sh->seqno()-1;
					}
				}*/
                //double loss__ = double(nlost_) / datatrain_;
                double loss__ = double(loss) / ((1.0/rate_interval_)*double(now_-prets_));
                
				if(!loss_has_been_alert){
					if(loss__>0){ //lossthreshold_
						lossnotify(seqno_, nlost_, loss__);
					}
				}
				++data_pkt_no_;

            }
		break;
		default:
		break;
	}
	
    fprintf(pFile3, "%0.6f %d %d %d %s %0.8f %d %d %d\n", now_, session_, sh->seqno(), seqno_, packetTypeAsStr(sh->type()), rate_interval_, sh->m_, data_pkt_no_, probing_pkt_no_);
    double dispersion__ = double(now_-prets_);
    double dispersion_ = 0.0f;
    if(dispersion__>EPSILON){
        if(!prets_)
            dispersion_ = 9999.0f;
        else{
			if(flagTvN_){
				dispersion_ = dispersion__-rate_interval_;
				if(dispersion_ < EPSILON){
					dispersion_ = 9999.0f;
				}
			}else{
				dispersion_ = dispersion__;
			}
        }
        
		if(s_dispersion_>0){
			s_dispersion_ = (1-owd_alpha_)*s_dispersion_+owd_alpha_*dispersion__;
		}else{
			s_dispersion_ = dispersion__;
		}
		
        pdis_ = (npkts_-1) % ndis_;
        dispersion[pdis_] = dispersion_;
        fprintf(pFile4, "%0.4f,%d,%s,%0.6f,%0.6f,%0.6f,%0.6f\n", now_, seqno_, packetTypeAsStr(sh->type()), s_dispersion_, dispersion[pdis_], rate_interval_, current_owd_);
        prets_ = now_;
    }
    
	last_packet_time_ = now_;
    
	switch(sh->type()){
		case SRMS_IPROBE:
		case SRMS_PROBE:
            {
				recv_probeSent(pkt);
                estimationOWD(now_, seqno_, sh->ts(), probetrain_);
                estimationQSize(now_, seqno_, pdis_, probetrain_, dispersion_);
                if( owdMatrixLeng >= probetrain_){
                //if(  sh->m_==0 ){
                //if(  sh->m_ == 0 ){
                    fprintf(pFile11, "%0.4f %d %d %d %d %d %d\n", now_ , seqno_, state_, probetrain_, datatrain_, qsMatrixLeng, owdMatrixLeng);
                    timeout(FULL);
                }
            }
            break;
		case SRMS_DATA:
            {
                estimationOWD(now_, seqno_, sh->ts(), datatrain_);
                estimationQSize(now_, seqno_, pdis_, datatrain_, dispersion_);
                if( owdMatrixLeng >= datatrain_ ){
                    fprintf(pFile11, "%0.4f %d %d %d %d %d %d\n", now_ , seqno_, state_, probetrain_, datatrain_, qsMatrixLeng, owdMatrixLeng);
                    timeout(FULL);
                }
            }
            break;
		default:
		break;
	}
	
	expected_ = seqno_ + 1;
    loss_rate_ = 0.0;
    Packet::free(pkt);
}

void SrmSinkAgent::lossnotify(int seqno__, int loss_no__, double loss__)
{
	Packet *pkt = allocpkt();
	hdr_srms *sh = hdr_srms::access(pkt);
    hdr_rtp *rh = hdr_rtp::access(pkt);
	sh->seqno() = seqno__;
	sh->session_ = session_;
    rh->flags() = 1;
    rh->seqno() = seqno__;
    hdr_cmn::access(pkt)->timestamp() = now();
    sh->flag() = SRMS_CONTROL;
    sh->type() = SRMS_LOSS;    
	sh->ts() = now();
	sh->rateInterval() = rate_interval_;
	sh->FS_ = FS_;
	sh->QFS_ = QFS_;
	sh->P_ = P_;
    sh->loss_ = loss__;
    target_->recv(pkt, (Handler*)0);
	loss_has_been_alert = true;
}

void SrmSinkAgent::output()
{
	Packet *pkt = allocpkt();
	hdr_srms *sh = hdr_srms::access(pkt);
    hdr_rtp *rh = hdr_rtp::access(pkt);
	sh->seqno() = maxseqno_;
    rh->flags() = 0;
    rh->seqno() = maxseqno_;
    hdr_cmn::access(pkt)->timestamp() = now();    
	
	sh->ts() = now();
	sh->rateInterval() = rate_interval_;
	sh->FS_ = FS_;
	sh->QFS_ = QFS_;
	sh->P_ = P_;
	sh->loss_ = loss_rate_;
	sh->session_ = session_;
	
	switch(state_){
		case INITIAL_ABWE_STATE:
			sh->flag() = SRMS_CONTROL;
			sh->type() = SRMS_IPROBEACK;
			//sh->rateInterval() = 0.00160000;
			//changeState(INITIAL_ABWE_STATE);
			//sh->seqno() = probe_curr_seqno_;
			break;
		case EXTRA_ABWE_STATE:
			sh->flag() = SRMS_CONTROL;
			sh->type() = SRMS_PROBEACK;
            sh->rateInterval() = snd_rate_interval_;
			//sh->seqno() = probe_curr_seqno_;
			//changeState(EXTRA_ABWE_STATE);
			break;
		case NORMAL_STATE:
			sh->flag() = SMRS_MSG;
			sh->type() = SRMS_DATACK;
			//sh->seqno() = snd_curr_seqno_;
			//changeState(PENDING_STATE);
			break;
		case CONGESTION_AVOIDANCE_STATE:
			sh->flag() = SRMS_CONTROL;
			sh->type() = SRMS_LOSS;
			//changeState(PENDING_STATE);
			break;
		default:
			break;
	}

	
	target_->recv(pkt, (Handler*)0);  
    changeState(PENDING_STATE);
}

void SrmSinkAgent::timeout(srmsink_response reason) 
{
    total_pktsizeno_ = data_pkt_no_ + probing_pkt_no_;
    if(total_pktsizeno_<2)
        return;
    
    now_ = now();
	owdtrend(now_);
    qstrend(now_);
	int seqno_dur_ = seqno_ - preseqno_;
	if(state_==INITIAL_ABWE_STATE||state_==EXTRA_ABWE_STATE){
		seqno_dur_ = probetrain_;
		nlost_ = (probetrain_-probing_pkt_no_);
	}else{
		seqno_dur_ = datatrain_;
		nlost_ = (datatrain_-data_pkt_no_);
	}
	
	if(seqno_dur_)
		loss_rate_ = double(nlost_)/(seqno_dur_);
	else
		loss_rate_ = 0.0;
	
    total_dispersion_ = last_packet_time_ - pretime_;	
	
    if(total_dispersion_>EPSILON){
        rate_interval_ = total_dispersion_/(total_pktsizeno_-1);
    }else{
        rate_interval_ = min_rate_interval_;
    }
    
	fprintf(pFile2, "%0.6f %d %s %d %0.4f %0.6f %d %0.6f %0.6f %d %d %0.6f\n", now_, reason, stateAsStr(state_), preseqno_, pretime_, last_packet_time_, total_dispersion_, total_pktsizeno_, double(rate_interval_), nlost_, seqno_dur_, double(loss_rate_)); 
	output();
    
	preseqno_ = seqno_;	
    data_pkt_no_ = 0;
    probing_pkt_no_ = 0;   
	nlost_ = 0;
	//pretime_ = 0;
    //pretime_ = now_;
	qsMatrixLeng = 0;
	owdMatrixLeng = 0;
	total_dispersion_ = 0;

}

void SrmSinkAgent::recv_probeSent(Packet *pkt) {
	if(rowd_>0){
		hdr_srms *sh = hdr_srms::access(pkt);
		if(sh->type()==SRMS_IPROBE||sh->type()==SRMS_PROBE){
			//if(sh->m_){
				timer_qs_.force_cancel();
				timer_qs_.resched(rowd_);	
			//}else{
			//	timer_qs_.force_cancel();
			//}
		}

	}
	else{
		fprintf(stderr,"%f, SrmSinkAgent(%s)::recv_probeSent() - rowd_ is 0!\n", now(), name());
		fflush(stdout);
		abort();
	}
}

int SrmSinkAgent::command(int argc, const char*const* argv)
{
    return (Agent::command(argc, argv));
}

/* Return a string representation of a type */
const char* SrmSinkAgent::stateAsStr(srms_state state){
	if (state < SRMS_NUM_STATES && state >= 0)
		return state_str_[state];
	else
		return "UNKNOWN";
}
/* Return a string representation of a type */
const char* SrmSinkAgent::packetTypeAsStr(srms_packet_type type){
	if (type < SRMS_NUM_PTYPES && type >= 0)
		return ptype_str_[type];
	else
		return "UNKNOWN";
}

inline void SrmSinkAgent::changeState(srms_state new_state){
	old_state_ = state_;

    if(old_state_!=new_state){
        pretime_ = now();
    }
	
	switch (new_state){
	case INITIAL_ABWE_STATE:
	case EXTRA_ABWE_STATE:
		timer_qs_.force_cancel();
		//timer_qs_.resched(rowd_);
		loss_has_been_alert = false;
		break;
	case NORMAL_STATE:
		timer_qs_.force_cancel();
		break;
	case CONGESTION_AVOIDANCE_STATE:
		//timer_qs_.force_cancel();
		break;
	case PENDING_STATE:
        //pretime_ = 0;
		timer_qs_.force_cancel();
		break;
	default:
		break;
	}
    
    state_ = new_state;
}


void SRMSQSTimer::expire(Event* /*e*/) {
    a_->timeout(EXPIRE);
}