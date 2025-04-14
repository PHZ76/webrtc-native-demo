#pragma once

#include "rtp_source.h"
#include <list>

class H264RtpSource : public RtpSource
{
public:
	H264RtpSource(uint32_t ssrc, uint32_t payload_type);
	virtual ~H264RtpSource();

	void InputFrame(uint8_t* frame_data, size_t frame_size);

private:
	void HandleSPSFrame(uint8_t* frame_data, size_t frame_size);
	void HandlePPSFrame(uint8_t* frame_data, size_t frame_size);
	void HandleIDRFrame(uint8_t* frame_data, size_t frame_size);
	void HandleRefFrame(uint8_t* frame_data, size_t frame_size);
	void BuildRtp(uint8_t* frame_data, size_t frame_size, std::list<RtpPacketPtr>& rtp_pkts);
	void BuildRtpSTAPA(uint8_t* frame_data, size_t frame_size, std::list<RtpPacketPtr>& rtp_pkts);
	void BuildRtpFUA(uint8_t* frame_data, size_t frame_size, std::list<RtpPacketPtr>& rtp_pkts);

	std::shared_ptr<uint8_t> sps_;
	std::shared_ptr<uint8_t> pps_;
	uint32_t sps_size_ = 0;
	uint32_t pps_size_ = 0;
	uint32_t max_rtp_payload_size_ = 0;
	uint32_t timestamp_ = 0;
};

