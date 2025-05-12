#pragma once

#include "rtcp.h"
#include <map>
#include <vector>

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

	bool Parse(uint8_t* pkt, size_t size);

	void OnSenderReportRecord(uint32_t last_sr);
	bool GetLostSeq(uint32_t ssrc, std::vector<uint16_t>& lost_seqs);

	uint32_t GetLossRate(uint32_t ssrc);
	uint32_t GetRTT(uint32_t ssrc);

private:
	void OnReceiverReport(uint8_t* payload, size_t size);
	void OnNack(uint8_t* payload, size_t size);
	void OnTransportFeedback(uint8_t* payload, size_t size);
	void OnFir(uint8_t* payload, size_t size);
	void OnPLI(uint8_t* payload, size_t size);

	RtcpHeader rtcp_header_ = {};
	std::map<uint32_t, uint64_t> last_sr_records_;
	std::map<uint32_t, std::vector<uint16_t>> nack_lost_seqs_;
	std::map<uint32_t, uint32_t> rtt_;
	std::map<uint32_t, uint32_t> loss_rate_;

	uint64_t last_print_time_ = 0;
};