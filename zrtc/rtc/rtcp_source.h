#pragma once

#include "rtcp.h"

class RtcpSource
{
public:
	RtcpSource(uint32_t ssrc);
	virtual ~RtcpSource();

	void SetSenderReportInterval(uint64_t sr_interval);
	void SetRtpTimestamp(uint32_t timestamp);
	void SetNtpTimestamp(uint64_t timestamp);
	void OnSendRtp(uint32_t rtp_packet_size, uint32_t timestamp);

	RtcpPacketPtr BuildSR();

private:
	bool CheckSendSR();

	uint32_t ssrc_ = 0;
	uint64_t sr_interval_ = 0;
	uint64_t last_build_sr_time_ = 0;
	uint32_t rtp_timestamp_ = 0;
	uint64_t ntp_timestamp_ = 0;
	uint32_t ntp_mword_ = 0;
	uint32_t ntp_lword_ = 0;
	uint32_t rtp_packet_count_ = 0;
	uint32_t rtp_octet_count_ = 0;
};
