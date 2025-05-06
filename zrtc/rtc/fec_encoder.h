#pragma once

#include "rtc_common.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/include/module_fec_types.h"

class FecEncoder
{ 
public:
	FecEncoder(uint32_t media_ssrc, uint32_t fec_ssrc, uint32_t fec_payload_type);
	virtual ~FecEncoder();

	void UpdateLossRate(uint32_t loss_rate);
	void AddRtpPacket(std::shared_ptr<RtpPacket> rtp_packet);
	
	std::list<std::shared_ptr<RtpPacket>> GetFecPackets();

private:
	uint32_t media_ssrc_ = 0;
	uint32_t fec_ssrc_ = 0;
	uint32_t fec_payload_type_ = 0;
	uint16_t fec_seq_ = 1;
	uint32_t loss_rate_ = 0;
	uint32_t smoothed_loss_rate_ = 0;
	std::unique_ptr<webrtc::ForwardErrorCorrection> fec_;
	webrtc::ForwardErrorCorrection::PacketList media_packets_;
	std::list<webrtc::ForwardErrorCorrection::Packet*> fec_packets_;
};