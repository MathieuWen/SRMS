#ifndef srms_sink_h
#define srms_sink_h
#include <stdio.h>
#include "udp.h"
#include "ip.h"
#include "agent.h"
#include "srms_types.h"
#include "srms_packets.h"
#include "packet.h"
#include "timer-handler.h"


enum srmsink_response {
	EXPIRE, FULL, LOSS, CUTIN
};

class SrmSinkAgent;
class SRMSQSTimer : public TimerHandler {
public: 
    SRMSQSTimer(SrmSinkAgent *a) : TimerHandler() { a_ = a; }
protected:
    virtual void expire(Event *e);
    SrmSinkAgent *a_;
};

class SrmSinkAgent : public UdpAgent {
public:
	SrmSinkAgent();
	SrmSinkAgent(packet_t);
	virtual ~SrmSinkAgent()
	{
		free(qsMatrix);
		free(dispersion);
		free(owd);
		fclose(pFile);
		fclose(pFile2);
		fclose(pFile3);
		fclose(pFile4);
		fclose(pFile5);
		fclose(pFile6);
		fclose(pFile7);
		fclose(pFile8);
		fclose(pFile9);
		fclose(pFile10);
		fclose(pFile11);
	}
	virtual void timeout(srmsink_response reason);
	virtual int command(int argc, const char*const* argv);
	virtual void recv(Packet* pkt, Handler*);
protected:
	static char* state_str_[SRMS_NUM_STATES];
	static char* ptype_str_[SRMS_NUM_PTYPES];  
    virtual void owdtrend(double now);
    virtual void qstrend(double now);
    virtual void estimationOWD(double now__, int seqno__, double ts__, int owdtrain__);
    virtual void estimationQSize(double now, int seqno__, int pdis, int qsztrain__, double dispersion__);
	inline double now() { return (&Scheduler::instance() == NULL ? 0 : Scheduler::instance().clock()); }
	virtual void recv_probeSent(Packet *pkt);
	void changeState(srms_state new_state);
	
	/* OTcl binding of variables */
	virtual void delay_bind_init_all();
	virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);	
	void output();
	void lossnotify(int seqno__, int loss_no__, double loss__);
    void init();
    int round(double x);
	bool fequal(double a, double b);
	srms_state state_;
	srms_state old_state_;
	int nlost_;       //�����ʥ]�ƶq
    int npkts_;             //�����쪺�ʥ]�ƶq
    int npkts2_;             //�����쪺�ʥ]�ƶq
    int flagTvN_;
    int ndis_;
    int pdis_;
    int expected_;
    int bytes_;      //�����쪺�ʥ]�`����(bytes)
    int seqno_;             //�ʥ]�Ǹ�
	int preseqno_;
	int maxseqno_;
    int trainSize_;
    int session_;
    double last_packet_time_; //�W�@���ʥ]�����ɶ�
    double interval_;
	double rate_interval_;
	double min_rate_interval_;
	double FS_;
	double QFS_;
	double P_; // congestion levels
    double snd_interval_;
    double snd_rate_interval_;
    double loss_rate_;
    double lossthreshold_;
    //double dispersion[100];
    double *dispersion;
    //double timestamp_s[100];
    double *owd;
    int *qsMatrix;
    int qsMatrixLeng;
    int owdMatrixLeng;
    int probetrain_;
    int datatrain_;
	bool loss_has_been_alert;
    char buf[100];
    FILE *pFile, *pFile2, *pFile3, *pFile4, *pFile5, *pFile6, *pFile7, *pFile8, *pFile9, *pFile10, *pFile11;          
    double now_;
    double sum_ipdv_;     //jitter�`�M,�i�H�ΨӺ�average jitter
    double sum_owd_;      //one way delay�`�M,�i�H�ΨӺ�average owd            
    double current_owd_;  //�ثe�Ҷq���쪺one way delay
    double current_ipdv_; //�ثe�Ҷq���쪺jitter
	double sowd_;
	double rowd_;
	double owdvar_;
	double owd_alpha_;
	double owd_beta_;
	double owd_K_;
    double prets_; 
    double min_owd_;      //�̤p��one way delay ��
    double max_owd_;      //�̤j��one way delay ��
    double min_owd_group_;
    double max_owd_group_;
    double avg_owd_group_;
    double previous_owd_; //�W�@���q���쪺one way delay
	double pretime_;
	double total_dispersion_;
	double s_dispersion_;
	volatile int probing_pkt_no_;
	volatile int data_pkt_no_;
	int total_pktsizeno_;
    SRMSQSTimer timer_qs_;
	const char* stateAsStr(srms_state state);
	const char* packetTypeAsStr(srms_packet_type type);
};

#endif