#ifndef ns_srms_packets_h
#define ns_srms_packets_h
#include "srms_types.h"

struct hdr_srms 
{
	srms_packet_flag flag_;
	srms_packet_type type_;
	int seqno_;
	int session_;
	double rate_interval_;
	double ts_;
	double loss_;
	double FS_;
	double QFS_;
	double P_; // congestion levels
	int m_;
	static int offset_;
	inline static int& offset() { return offset_; }
	inline static hdr_srms* access(const Packet* p) {
	   return (hdr_srms*) p->access(offset_);
	}

	srms_packet_flag& flag() {return (flag_);}
	srms_packet_type& type() {return (type_);}
	int& seqno() {return (seqno_);}
	double& rateInterval() {return (rate_interval_);}
	double& ts() {return (ts_);}
};


#endif