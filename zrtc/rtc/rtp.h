#pragma once

#include <cstdint>
#include <memory>

static const uint8_t  RTP_VERSION = 2;
static const uint32_t RTP_HEADER_SIZE = 12;
static const uint32_t RTX_EXTENSION_HEADER_SIZE = 4;

enum RtpExtensionType
{
	RTP_EXTENSION_TWCC = 1,
};

struct RtpHeader
{
	uint8_t version;
	uint8_t padding;
	uint8_t extension;
	uint8_t csrc_count;
	uint8_t marker;
	uint8_t payload_type;
	uint16_t sequence;
	uint32_t timestamp;
	uint32_t ssrc;
};

struct RtpPacket
{
	RtpPacket()
		: data(new uint8_t[1500], std::default_delete<uint8_t[]>())
	{
		memset(data.get(), 0, 1500);
	}

	uint32_t ssrc = 0;
	uint32_t timestamp = 0;
	uint16_t sequence = 0;
	uint8_t  marker = 0;

	std::shared_ptr<uint8_t> data;
	uint32_t data_size  = 0;
	uint8_t  frame_type = 0;
	uint8_t  is_rtx_ = 0;
	uint8_t  is_fec_ = 0;
};

using RtpPacketPtr = std::shared_ptr<RtpPacket>;

static bool IsRtpPacket(const uint8_t* data, size_t size)
{
	if (size >= 12 && (data[0] & 0x80) && !(data[1] >= 192 && data[1] <= 223)) {
		return true;
	}
	return false;
}