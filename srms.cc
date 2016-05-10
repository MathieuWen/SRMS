#include <stdlib.h>
#include <math.h>
#include "ip.h"
#include "srms.h"
#include "rtp.h"
#include <string.h>
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

//string representation of types
char* SrmsAgent::state_str_[SRMS_NUM_STATES] =
{"INITIAL_ABWE_STATE", "EXTRA_ABWE_STATE", "NORMAL_STATE", "PENDING_STATE", "CONGESTION_AVOIDANCE_STATE"};
char* SrmsAgent::ptype_str_[SRMS_NUM_PTYPES] =
{"SRMS_PROBE","SRMS_IPROBE", "SRMS_DATA", "SRMS_PROBEACK", "SRMS_IPROBEACK", "SRMS_DATACK", "SRMS_LOSS"};

// SRMSAgent OTcl linkage class
static class SRMSClass : public TclClass 
{
public:
	SRMSClass() : TclClass("Agent/UDP/SRMS") {}
	TclObject* create(int, const char*const*) 
	{
		return (new SrmsAgent());
	}
} class_srms_agent;


SrmsAgent::SrmsAgent(): UdpAgent(),
probe_has_sent(false),
rate_has_down(false),
probe_curr_seqno_(0),
snd_curr_seqno_(0),
curr_max_seqno_(0),
probe_sent_no_(0),
data_sent_no_(0),
congestion_rank_(1),
idle2WakeUp_(0.3),
loss_rate_(0.0),
state_(PENDING_STATE),
bandwidth_scale_ptr(bandwidth_scale_),
old_bandwidth_scale_ptr(bandwidth_scale_)
{
    init();
}

SrmsAgent::SrmsAgent(packet_t type) : UdpAgent(type),
probe_has_sent(false),
rate_has_down(false),
probe_curr_seqno_(0),
snd_curr_seqno_(0),
curr_max_seqno_(0),
probe_sent_no_(0),
data_sent_no_(0),
idle2WakeUp_(0.3),
congestion_rank_(1),
loss_rate_(0.0),
state_(PENDING_STATE),
bandwidth_scale_ptr(bandwidth_scale_),
old_bandwidth_scale_ptr(bandwidth_scale_)
{
    init();
}

void SrmsAgent::init()
{
	maxsession_ = -1;
    alpha__ = 1/8;
	session_ = 0;
	RANKthreshold_ = 6;
    has_been_down_ = 0;
	down_up_ = false;
	flagStop__ = false;
	timer_snd_ = new SRMSndTimer(this);
	timer_idle_ = new SRMSidleTimer(this);
	timer_refresh_ = new SRMSrefreshTimer(this);
	bind("packetSize_", &size_);
	bind("probingTrain_", &probingTrain_);
	bind("FSthreshold_", &FSthreshold_);
	bind("QFSthreshold_", &QFSthreshold_);
	bind("lossthreshold_", &lossthreshold_);
	bind("snd_interval_", &snd_interval_);
	bind("abwe_", &abwe_);
	bind("Pminthreshold_", &Pminthreshold_);
	bind("Pmaxthreshold_", &Pmaxthreshold_);
	bind("idle2WakeUp_", &idle2WakeUp_);
	bind("loss_rate_", &loss_rate_);
    
    static unsigned int s_agent_ID = 0;
    char strTmp[30];
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "recv_source");
    pFile = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "snd_source");
    pFile2 = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "intervalTB");
	pFile3 = fopen(strTmp, "w");
    sprintf(strTmp, "agent%d_%s.csv", s_agent_ID, "idle2WakeUp");
	pFile4 = fopen(strTmp, "w");
    s_agent_ID++;
	bandwidthAlloc();
	
	double rnk_[rank_scale_]={0.05, 0.1, 0.15, 0.2, 0.55, 0.75, 0.85, 1.0};
	memcpy(ranktable_, rnk_, sizeof(rnk_));    
}

/* OTcl binding of variables */
void SrmsAgent::delay_bind_init_all(){
	delay_bind_init_one("packetSize_");
	delay_bind_init_one("probingTrain_");
	delay_bind_init_one("FSthreshold_");
	delay_bind_init_one("QFSthreshold_");
	delay_bind_init_one("idle2WakeUp_");
	delay_bind_init_one("Pmaxthreshold_");
	delay_bind_init_one("Pminthreshold_");
	delay_bind_init_one("lossthreshold_");
	delay_bind_init_one("snd_interval_");
	delay_bind_init_one("abwe_");
	delay_bind_init_one("loss_rate_");
	Agent::delay_bind_init_all();
}

int SrmsAgent::delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer){
	if (delay_bind(varName, localName, "packetSize_", &size_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "probingTrain_", &probingTrain_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "FSthreshold_", &FSthreshold_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "QFSthreshold_", &QFSthreshold_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "idle2WakeUp_", &idle2WakeUp_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "Pmaxthreshold_", &Pmaxthreshold_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "Pminthreshold_", &Pminthreshold_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "lossthreshold_", &lossthreshold_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "snd_interval_", &snd_interval_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "abwe_", &abwe_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "loss_rate_", &loss_rate_, tracer)) return TCL_OK;
	return Agent::delay_bind_dispatch(varName, localName, tracer);
}

SrmsAgent::~SrmsAgent()
{
	delete timer_snd_;
	delete timer_idle_;
	delete [] bandwidth_;
	delete [] bandwidth_interval_;
	delete [] ranktable_;
    fclose(pFile);
    fclose(pFile2);
    fclose(pFile3);
    fclose(pFile4);
}

void SrmsAgent::bandwidthAlloc()
{
	int bw[bandwidth_scale_]={128*KBPS, 256*KBPS, 512*KBPS, MBPS, 2*MBPS, 3*MBPS, 4*MBPS, 5*MBPS, 6*MBPS, 7*MBPS, 8*MBPS, 9*MBPS, 10*MBPS, 11*MBPS, 12*MBPS, 13*MBPS, 14*MBPS, 15*MBPS, 16*MBPS, 20*MBPS};
	memcpy(bandwidth_, bw, sizeof(bw));
	double s=0.0;
	for(int i=0;i<bandwidth_scale_;i++){
		s = bandwidth_[i]/size_/8;
		bandwidth_interval_[i]= 1.0/s;
		fprintf(pFile3, "%d %d %0.8f\n", i, bandwidth_[i], bandwidth_interval_[i]);
	}
	//debug("%f, SRMS(%s)::bandwidthAlloc() ...\n", now(), name());
}

int SrmsAgent::command(int argc, const char*const* argv)
{
	if(argc == 2){
		if(strcmp(argv[1], "stop") == 0) {
			stop();
			return (TCL_OK);
		}
	}else if (argc == 4) {
		if (strcmp(argv[1], "send") == 0) {
			PacketData* data = new PacketData(1 + strlen(argv[3]));
			strcpy((char*)data->data(), argv[3]);
			sendmsg(atoi(argv[2]), data);
			return (TCL_OK);
		}
	} else if (argc == 5) {
		if (strcmp(argv[1], "sendmsg") == 0) {
			PacketData* data = new PacketData(1 + strlen(argv[3]));
			strcpy((char*)data->data(), argv[3]);
			sendmsg(atoi(argv[2]), data, argv[4]);
			return (TCL_OK);
		}
	}
	
   return Agent::command(argc, argv);
}

void SrmsAgent::stop()
{
	timer_snd_->force_cancel();
	timer_idle_->force_cancel();
	flagStop__ = true;
}

/* Receive a packet
 * arg: pkt     - Packet received
 *      handler - handler
 */
void SrmsAgent::recv(Packet *pkt, Handler*)
{
	if(flagStop__)	return;
    hdr_srms* rh = hdr_srms::access(pkt);
    int rank_ = getRank(rh->P_);
	loss_rate_ = rh->loss_;
	int fdsession_ = rh->session_;
	
	switch(rh->type()){
		case SRMS_IPROBEACK:
			{
				rateControl(rh->rate_interval_);
				if(rh->FS_>FSthreshold_){
					changeState(INITIAL_ABWE_STATE);
				}else{
                    abwe_ = snd_interval_;
					changeState(NORMAL_STATE);
				}
			}
			break;
		case SRMS_PROBEACK:
			{
				if(rh->QFS_>QFSthreshold_ && rh->FS_>FSthreshold_){ //||rh->FS_>FSthreshold_
					//rateAdapter(-1);
				}else{
                    rateAdapter(+1);
                }
                abwe_ = snd_interval_;
                //rateControl(rh->rate_interval_);
                
				changeState(NORMAL_STATE);
                //probe_has_sent = false;
			}
			break;
		case SRMS_DATACK:
			{
				//if(fdsession_ <= maxsession_)
				//	goto free;
				if(rh->loss_>lossthreshold_){
					changeState(CONGESTION_AVOIDANCE_STATE);
					
				}else{
					if(rh->loss_<EPSILON){
						if(congestion_rank_<double(rank_)){
							congestion_rank_ = double(rank_);
						}
					}	
					if(rh->QFS_>QFSthreshold_ && rh->FS_>FSthreshold_){ //||rh->FS_>FSthreshold_
						changeState(CONGESTION_AVOIDANCE_STATE);
					}else{
                        /*
                        if(rh->P_>Pmaxthreshold_){
                            changeState(CONGESTION_AVOIDANCE_STATE);
                        } else if(Pminthreshold_>rh->P_){
                            if(!probe_has_sent){
                                rateAdapter(+1);
                                changeState(EXTRA_ABWE_STATE);
                            }
                        }*/
                        
                        if(!probe_has_sent){
                            rateAdapter(+1);
                            changeState(EXTRA_ABWE_STATE);
                        }
                        has_been_down_=0;
                        
					}
				}
			}
			break;
		case SRMS_LOSS:
			{
				//if(fdsession_<maxsession_)
					//goto free;
                //if(!rate_has_down){
				if(has_been_down_==0){
					changeState(CONGESTION_AVOIDANCE_STATE);
                    has_been_down_++;
				}
                    
                    //rate_has_down = true;
                //}
                
			}
			break;
		default:
			break;
	}
	
	if(maxsession_<fdsession_)
		maxsession_ = fdsession_;	
	
	switch(state_){
		case INITIAL_ABWE_STATE:
		case NORMAL_STATE:
		break;
		case EXTRA_ABWE_STATE:

		break;
		case CONGESTION_AVOIDANCE_STATE:
			congestionAvoidance(rank_);
            abwe_ = snd_interval_;
			break;
		default:
			break;
	}
	
    int abwe__ = 0;
    if(abwe_)
        abwe__ = 1.0/double(abwe_)*8*size_;
fprintf(pFile, "%0.6f %d %d %0.8f %s %0.4f %0.4f %0.4f %0.4f %d %d %0.8f\n", now(), fdsession_, rh->seqno(), rh->rateInterval(), packetTypeAsStr(rh->type()), rh->FS_, rh->QFS_, rh->loss_, rh->P_, rank_, abwe__,double(abwe_));
    
free:	
    Packet::free(pkt);
}

/* Send a packet,
 * If this is the first packet to send, initiate a connection.
 * arg: nbytes - number of bytes in packet (-1 -> infinite send)
 *      flags  - Flags:
 *                 "MSG_EOF" will close the connection when all data
 *                  has been flag
 */
//void SrmsAgent::sendmsg(int nbytes, const char* flags)
void SrmsAgent::sendmsg(int nbytes, AppData* data, const char* flags)
{
	//curr_max_seqno_ += nbytes/1468;
	//fprintf(pFile2, "%d %d", curr_max_seqno_, nbytes);
	changeState(INITIAL_ABWE_STATE);
}

Packet* SrmsAgent::newPacket(){
	Packet *pkt = allocpkt();
	//hdr_rtp *rh = hdr_rtp::access(pkt);
	//rh->flags() = 0;
	//rh->seqno() = snd_curr_seqno_;
	
    hdr_srms *sh = hdr_srms::access(pkt);
    sh->flag() = SRMS_CONTROL;
    sh->seqno() = snd_curr_seqno_;	
    //hdr_cmn* ch = hdr_cmn::access(pkt);
    //ch->size() = mtu_;	
	
	return pkt;
}


/* Send a packet.
 * arg: 
 *                  
 */
void SrmsAgent::output()
{
	if(flagStop__)	return;
send:	
	Packet *pkt = allocpkt();
	hdr_srms *sh = hdr_srms::access(pkt);
	hdr_rtp *rh = hdr_rtp::access(pkt);
    rh->flags() = 0;
    rh->seqno() = snd_curr_seqno_;
    hdr_cmn::access(pkt)->timestamp() = now();
	switch(state_){
		case INITIAL_ABWE_STATE:
			sh->flag() = SRMS_CONTROL;
			sh->type() = SRMS_IPROBE;
			sh->seqno() = probe_sent_no_;
			break;
		case EXTRA_ABWE_STATE:
			sh->flag() = SRMS_CONTROL;
			sh->type() = SRMS_PROBE;
			sh->seqno() = probe_sent_no_;
			break;
		case NORMAL_STATE:
			sh->flag() = SMRS_MSG;
			sh->type() = SRMS_DATA;
			sh->seqno() = data_sent_no_;
			break;
		case CONGESTION_AVOIDANCE_STATE:
			break;
		default:
			break;
	}
	
	sh->ts() = now();
	sh->rateInterval() = snd_interval_;
	sh->session_ = session_;
	snd_curr_seqno_++;
	switch(state_){
		case INITIAL_ABWE_STATE:
		case EXTRA_ABWE_STATE:
			{
				probe_sent_no_++;
				
			}
			break;
		case NORMAL_STATE:
			data_sent_no_++;
			break;
		case CONGESTION_AVOIDANCE_STATE:
			break;
		default:
			break;
	}
    
    send_packetSent(pkt);
    
	
    sh->m_ = 1;
	
    if(probe_sent_no_==probingTrain_){
        sh->m_ = 0;
    }else if(probe_sent_no_>probingTrain_){
		goto free;
	}
    
    target_->recv(pkt, (Handler*)0);
    //Agent::send(pkt, 0);
	//send(pkt, 0);
	fprintf(pFile2, "%0.6f %d %d %d %s %0.6f %d\n", sh->ts(), sh->session_, rh->seqno(), sh->seqno(), packetTypeAsStr(sh->type()), double(snd_interval_), sh->m_);
	
	
	if(probe_sent_no_>=probingTrain_){
	debug("%f, SRMS(%s)::output() - state(%s) probe(%d) has sent %d probe, IR %0.6f\n",
	  now(), name(), stateAsStr(state_), sh->seqno(), probe_sent_no_, double(snd_interval_));		
		
		//snd_curr_seqno_ --;
        if(state_==INITIAL_ABWE_STATE){
            probe_sent_no_ = 0;
            changeState(PENDING_STATE);
        }else if(state_==EXTRA_ABWE_STATE){
            probe_sent_no_ = 0;
            //rateAdapter(-1);
            changeState(FRESH_STATE);            
        }
		//goto free;
	}
    
   
	
	return;
free:	
	Packet::free(pkt);
}


/* A(n) DATA/PROBE packet has been sent (sender)
 * arg: pkt        - packet sent
 *      moreToSend - true if there exist more data to send
 *      dataSize   - size of data sent
 */
void SrmsAgent::send_packetSent(Packet *pkt) {
	if (snd_interval_ > 0)
		timer_snd_->resched(snd_interval_);
	else {
		fprintf(stderr,"%f, SRMS(%s)::send_packetSent() - snd_interval_ is 0!\n", now(), name());
		fflush(stdout);
		abort();
	}
};

void SrmsAgent::rateAdapter(int de_increase)
{
	int newbw_scale_ptr = bandwidth_scale_ptr+de_increase;
	int oldbw_scale_ptr = bandwidth_scale_ptr;
	/*
	if(de_increase<0){
		if(down_up_){
			return;
		}
	}
	*/
	if(newbw_scale_ptr < 0){
		bandwidth_scale_ptr = 0;
	}else if(newbw_scale_ptr>=bandwidth_scale_-1){
		bandwidth_scale_ptr = bandwidth_scale_-1;
	}else{  
        bandwidth_scale_ptr = newbw_scale_ptr;
	}
    
    if(old_bandwidth_scale_ptr!=oldbw_scale_ptr)
        old_bandwidth_scale_ptr = oldbw_scale_ptr;
    
	debug("%f, SRMS(%s)::rateAdapter() - bandwidth_scale_ptr changed from %d to %d\n",
	  now(), name(), oldbw_scale_ptr, bandwidth_scale_ptr);
	rateControl(bandwidth_interval_[bandwidth_scale_ptr]);
	
	/*
	if(de_increase<0){
		down_up_ = true;
	}else{
		down_up_ = false;
	}*/
}

void SrmsAgent::rateControl(double interval_)
{

	double old_interval_ = snd_interval_;
	if(interval_ > 0){
		snd_interval_ = interval_;
		int bwLevel = 0;
		for(int i=0;i<bandwidth_scale_;i++){
			if(bandwidth_interval_[i]<interval_){
				break;
			}
			bwLevel++;
		}
		bandwidth_scale_ptr = bwLevel - 1;
	debug("%f, SRMS(%s)::rateControl() - bandwidth_interval changed from %0.6f to %0.6f and ptr(%d)\n",
		now(), name(), old_interval_, interval_, bandwidth_scale_ptr);		
	}
	else{
		fprintf(stderr,"%f, SRMS(%s)::rateControl() - interval_ is 0!\n", now(), name());
	}
}

void SrmsAgent::flowControl()
{
}

void SrmsAgent::timeOut(int tno)
{
	switch (tno){
		case SRMS_TIMER_SND:
			output();
			break;
        case SRMS_TIMER_IDLE:
            {
                //rateAdapter(-1);
                //changeState(NORMAL_STATE);
                fprintf(pFile4, "%0.6f\n", now());
                debug("%f, SRMS(%s)::timeOut() - From idle to Normal\n", now(), name());                
            }
            break;
        case SRMS_TIMER_REFRESH:
            {
                timer_refresh_->force_cancel();
                rateAdapter(-1);
                changeState(NORMAL_STATE);    
            }
            break;
	}
}

int SrmsAgent::round(double x)
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

void SrmsAgent::congestionAvoidance(int rank__)
{
	rateAdapter(-1);
	changeState(NORMAL_STATE);
#if 0
    //int normal_rank = SrmsAgent::round(congestion_rank_);
	int normal_rank = RANKthreshold_;
    int downStep = 1;

    if(normal_rank){
        if(normal_rank==rank__){
            downStep = 1;
        }else if(normal_rank<rank__){
			
            //fprintf(stderr,"%f, SRMS(%s)::congestionAvoidance() - normal_rank(%d) > rank__(%d) !\n", now(), name(), normal_rank, rank__);
			//normal_rank = rank__;
			
            //return;
            downStep = rank__-normal_rank;
        }
        
        debug("%f, SRMS(%s)::congestionAvoidance() - bandwidth_interval_ changed from %d to %d\n",
              now(), name(), bandwidth_scale_ptr, bandwidth_scale_ptr-downStep);
        rateAdapter(-1*downStep);
          
        changeState(NORMAL_STATE);
    }else{
        fprintf(stderr,"%f, SRMS(%s)::congestionAvoidance() - normal_rank <= 0!\n", now(), name());
    }
#endif	
}


int SrmsAgent::getRank(double ratio)
{
	int lo = 0;
	int hi = rank_scale_;
	int mid = 0;
	while(lo < hi){
		mid = (lo+hi)/2;
		if(ranktable_[mid]<ratio){
			lo = mid + 1;
		}else{
			hi = mid;
		}
	}
	return (lo<rank_scale_)?lo+1:rank_scale_;
}


/* Return a string representation of a type */
const char* SrmsAgent::stateAsStr(srms_state state){
	if (state < SRMS_NUM_STATES && state >= 0)
		return state_str_[state];
	else
		return "UNKNOWN";
}
/* Return a string representation of a type */
const char* SrmsAgent::packetTypeAsStr(srms_packet_type type){
	if (type < SRMS_NUM_PTYPES && type >= 0)
		return ptype_str_[type];
	else
		return "UNKNOWN";
}

inline void SrmsAgent::changeState(srms_state new_state){
	debug("%f, SRMS(%s)::changeState() - State changed from %s to %s\n",
	      now(), name(), stateAsStr(state_), stateAsStr(new_state));
	
	switch (new_state){
	case INITIAL_ABWE_STATE:
		timer_snd_->force_cancel();
        timer_idle_->force_cancel();
		timer_snd_->resched(0);
		session_++;
		break;
	case EXTRA_ABWE_STATE:
		timer_snd_->force_cancel();
        timer_idle_->force_cancel();
		timer_snd_->resched(snd_interval_);
		probe_has_sent = true;
		session_++;
		break;
	case NORMAL_STATE:
		timer_snd_->force_cancel();
        timer_idle_->force_cancel();
		//timer_snd_->resched(snd_interval_);
		timer_snd_->resched(snd_interval_);
		probe_has_sent = false;
		data_sent_no_ = 0;
		session_++;
		break;
	case CONGESTION_AVOIDANCE_STATE:
		//cancelTimers();
		timer_snd_->force_cancel();
		break;
	case PENDING_STATE:
		timer_snd_->force_cancel();
        timer_idle_->resched(idle2WakeUp_);
		break;
    case FRESH_STATE:
        timer_snd_->force_cancel();
        timer_refresh_->resched(0.01);
        break;
	default:
		break;
	}
    state_ = new_state;
}

/////////////////////////////////////////////////////////////////
void SRMSndTimer::expire(Event*)
{
	agent_->timeOut(SRMS_TIMER_SND);
}

void SRMSidleTimer::expire(Event*)
{
	agent_->timeOut(SRMS_TIMER_IDLE);
}

void SRMSrefreshTimer::expire(Event*)
{
	agent_->timeOut(SRMS_TIMER_REFRESH);
}


