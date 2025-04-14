#pragma once

#include "rtp_source.h"

class OpusRtpSource : public RtpSource
{
public:
	using SendPacketCallback = std::function<void(uint8_t* pkt, size_t size)>;

	OpusRtpSource(uint32_t ssrc, uint32_t payload_type);
	virtual ~OpusRtpSource();

	void InputFrame(uint8_t* frame_data, size_t frame_size);

private:
	uint32_t timestamp_ = 0;
};

