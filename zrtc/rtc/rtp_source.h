#pragma once

#include "rtc_common.h"
#include <chrono>

class RtpSource
{
public:
	using SendPacketCallback = std::function<void(std::list<RtpPacketPtr> rtp_pkts)>;

	RtpSource(uint32_t ssrc, uint8_t payload_type);
	virtual ~RtpSource();

	void SetTimestamp(uint32_t timestamp);
	void SetMarker(uint8_t marker);
	void SetSequence(uint32_t sequence);
	void BuildHeader(std::shared_ptr<RtpPacket> rtp_pkt);

	void SetSendPacketCallback(const SendPacketCallback& callback)
	{ 
		send_pkt_callback_ = callback; 
	}
protected:
	RtpHeader rtp_header_ = {};
	SendPacketCallback send_pkt_callback_;

	uint16_t sequence_ = 1;
	uint32_t clock_rate_ = 0;
};

