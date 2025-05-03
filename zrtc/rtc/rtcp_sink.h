#pragma once

#include "rtcp.h"
#include <map>

struct ReportBlock
{
	uint32_t ssrc = 0;
	uint8_t  fraction_lost = 0;
	uint32_t cumulative_packets_lost = 0;
	uint32_t exetened_highest_sequence_number = 0;
	uint32_t jitter = 0;
	uint32_t last_sr = 0;
	uint32_t delay_since_last_sr = 0;
};

class RtcpSink
{
public:
	RtcpSink();
	virtual ~RtcpSink();

	void OnSenderReportRecord(uint32_t last_sr);

	bool Parse(uint8_t* pkt, size_t size);

private:
	void OnReceiverReport(uint8_t* payload, size_t size);
	void OnRtpFeedback(uint8_t* payload, size_t size);

	RtcpHeader rtcp_header_;
	std::map<uint32_t, uint64_t> last_sr_records_;
};