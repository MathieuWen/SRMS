#include "packet.h"
#include "srms_packets.h"

int hdr_srms::offset_;

// SRMS Header Class 
static class SRMSHeaderClass : public PacketHeaderClass 
{
public:
    SRMSHeaderClass() : PacketHeaderClass("PacketHeader/SRMS", sizeof(hdr_srms)) 
    {
        bind_offset(&hdr_srms::offset_);
    }
} class_srmshdr;