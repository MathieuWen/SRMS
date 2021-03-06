#ifndef ns_srms_types_h
#define ns_srms_types_h

#define EPSILON (1.0E-8)
#define SRMS_TIMER_SND 0
#define SRMS_TIMER_IABWE 1
#define SRMS_TIMER_IDLE 2
#define SRMS_TIMER_REFRESH 3
#define MBPS 1000000
#define KBPS 1000
#define SRMS_NUM_STATES 5
#define SRMS_NUM_PTYPES 7

enum srms_state {
	INITIAL_ABWE_STATE, EXTRA_ABWE_STATE, NORMAL_STATE, PENDING_STATE, CONGESTION_AVOIDANCE_STATE, FRESH_STATE
};

enum srms_packet_flag {
	SRMS_CONTROL, SMRS_MSG
};

enum srms_packet_type {
	SRMS_PROBE, SRMS_IPROBE, SRMS_DATA, SRMS_PROBEACK, SRMS_IPROBEACK, SRMS_DATACK, SRMS_LOSS
};

#endif