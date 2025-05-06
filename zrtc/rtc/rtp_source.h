#pragma once

#include "rtc_common.h"
#include "fec_encoder.h"
#include <chrono>
#include <vector>

class RtpSource
{
public:
	using SendPacketCallback = std::function<void(std::list<RtpPacketPtr> rtp_pkts)>;

	RtpSource(uint32_t ssrc, uint8_t payload_type);
	virtual ~RtpSource();

	virtual void SetRtx(uint32_t rtx_ssrc, uint8_t payload_type);
	virtual void SetFec(uint32_t fec_ssrc, uint8_t payload_type);
	virtual void SetTimestamp(uint32_t timestamp);
	virtual void SetMarker(uint8_t marker);
	virtual void SetSequence(uint32_t sequence);
	virtual void BuildHeader(std::shared_ptr<RtpPacket> rtp_pkt);
	virtual void RetransmitRtpPackets(std::vector<uint16_t>& lost_seqs);
	virtual void SetSendPacketCallback(const SendPacketCallback& callback);
	virtual uint32_t GetTimestamp();
	virtual uint32_t GetSSRC();

	virtual void UpdateQoS(uint32_t rtt, uint32_t loss_rate);

protected:
	void UpdateRtpCache(std::list<RtpPacketPtr>& rtp_pkts);
	void GeneratedFecPacket(std::list<RtpPacketPtr>& rtp_pkts);

	RtpHeader rtp_header_ = {};
	SendPacketCallback send_pkt_callback_;
	uint16_t sequence_ = 1;
	uint32_t clock_rate_ = 0;

	uint16_t rtx_seq_ = 1;
	uint32_t rtx_ssrc_ = 0;
	uint32_t rtx_payloa_type_ = 0;
	std::vector<RtpPacketPtr> rtp_cache_;

	uint32_t fec_ssrc_ = 0;
	uint32_t fec_payloa_type_ = 0;
	std::shared_ptr<FecEncoder> fec_encoder_;

	uint32_t rtt_ = 0;
	uint32_t smooth_rtt_ = 0;
	uint32_t loss_rate_ = 0;
};

