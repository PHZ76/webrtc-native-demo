#pragma once

#include "rtcp.h"

class RtcpSink
{
public:
	RtcpSink(uint32_t ssrc);
	virtual ~RtcpSink();

};