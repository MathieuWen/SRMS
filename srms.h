#ifndef __NS_SRMS_H__
#define __NS_SRMS_H__

#include "udp.h"
#include "ip.h"
#include "srms_types.h"
#include "srms_packets.h"
#include "packet.h"
#include "timer-handler.h"


const int bandwidth_scale_ = 20;
const int rank_scale_ = 8;




class SrmsAgent;

/* The SndTimer models the time it takes to send a packet */
class SRMSndTimer: public TimerHandler 
{
protected:
	SrmsAgent *agent_;	//the owning agent
public:
	SRMSndTimer(SrmsAgent *agent) : TimerHandler() { agent_ = agent; }
	virtual void expire(Event *e);
};

class SRMSidleTimer: public TimerHandler 
{
protected:
	SrmsAgent *agent_;	//the owning agent
public:
	SRMSidleTimer(SrmsAgent *agent) : TimerHandler() { agent_ = agent; }
	virtual void expire(Event *e);
};

class SRMSrefreshTimer: public TimerHandler 
{
protected:
	SrmsAgent *agent_;	//the owning agent
public:
	SRMSrefreshTimer(SrmsAgent *agent) : TimerHandler() { agent_ = agent; }
	virtual void expire(Event *e);
};

class SrmsAgent: public UdpAgent	//class SrmsAgent: public UdpAgent
{
private:
	SRMSndTimer *timer_snd_;
	SRMSidleTimer *timer_idle_;
	SRMSrefreshTimer *timer_refresh_;
	srms_state state_;
	Packet* newPacket();
	
	//string representation of types
	static char* state_str_[SRMS_NUM_STATES];
	static char* ptype_str_[SRMS_NUM_PTYPES];
    int round(double x);
    void init();
protected:
	/* OTcl binding of variables */
	virtual void delay_bind_init_all();
	virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);	
	int probingTrain_;
	TracedDouble snd_interval_;
	TracedDouble abwe_;
	double FSthreshold_;
	double QFSthreshold_;
	double lossthreshold_;
	double Pmaxthreshold_;
	double Pminthreshold_;
    double idle2WakeUp_;
    TracedDouble congestion_rank_;
    double alpha__;
	inline double now() { return (&Scheduler::instance() == NULL ? 0 : Scheduler::instance().clock()); }
	int bandwidth_[bandwidth_scale_];
	int bandwidth_scale_ptr;
	int old_bandwidth_scale_ptr;
	double bandwidth_interval_[bandwidth_scale_];
	double ranktable_[rank_scale_];
	volatile int snd_curr_seqno_;
	volatile int ack_curr_seqno_;
	volatile int probe_curr_seqno_;
	volatile int probe_sent_no_;
	volatile int data_sent_no_;
	volatile int session_;
	volatile int maxsession_;
	bool down_up_;
    int has_been_down_;
	int curr_max_seqno_;
	TracedDouble loss_rate_;
	FILE *pFile, *pFile2, *pFile3, *pFile4;
	virtual void stop();
    void bandwidthAlloc();
	void rateControl(double interval_);
	void rateAdapter(int de_increase);
	void flowControl();
	void output();
	void changeState(srms_state new_state);
	void congestionAvoidance(int rank__ = 1);
	virtual void send_packetSent(Packet *pkt);
	int getRank(double ratio);
	int RANKthreshold_;
    bool probe_has_sent;
    bool rate_has_down;
	bool flagStop__;
	const char* stateAsStr(srms_state state);
	const char* packetTypeAsStr(srms_packet_type type);
public:
    SrmsAgent();
    SrmsAgent(packet_t);
    ~SrmsAgent();
    int command(int argc, const char*const* argv);
    //virtual void sendmsg(int nbytes, const char *flags = 0);
    virtual void sendmsg(int nbytes, AppData* data, const char *flags = 0);
	virtual void recv(Packet*, Handler*); 
	virtual void timeOut(int tno);
};


#endif
