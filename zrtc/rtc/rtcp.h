#pragma once

#include <cstdint>
#include <cstdint>
#include <memory>
#include <vector>

static const uint8_t RTCP_VERSION      = 2;
static const uint8_t RTCP_HEADER_SIZE  = 4;

static const uint8_t RTCP_PT_SENDER_REPORT   = 200;
static const uint8_t RTCP_PT_RECEIVER_REPORT = 201;
static const uint8_t RTCP_PT_RTPFB           = 205;
static const uint8_t RTCP_PT_PSFB            = 206;

static const uint8_t RTCP_PT_RTPFB_NACK      = 1;
static const uint8_t RTCP_PT_RTPFB_TWCC      = 15;
static const uint8_t RTCP_PT_PSFB_PLI        = 1;
static const uint8_t RTCP_PT_PSFB_FIR        = 4;

static const uint8_t RTCP_SENDER_REPORT_SIZE = 28;
static const uint8_t RTCP_BLOCK_SIZE = 24;

struct RtcpHeader
{
	uint8_t  version      : 2;
	uint8_t  padding      : 1;
	uint8_t  rc           : 5;
	uint8_t  payload_type;
	uint16_t length ;

	std::vector<uint8_t> Build()
	{
		std::vector<uint8_t> header(RTCP_HEADER_SIZE);
		header[0] = (version << 6) | (padding << 5) | rc;
		header[1] = payload_type;
		header[2] = length >> 8;
		header[3] = length & 0xff;
		return header;
	}
};

struct RtcpPacket
{
	RtcpPacket()
		: data(new uint8_t[1500], std::default_delete<uint8_t[]>())
	{
		memset(data.get(), 0, 1500);
	}

	std::shared_ptr<uint8_t> data;
	uint32_t data_size = 0;
};

using RtcpPacketPtr = std::shared_ptr<RtcpPacket>;

static bool IsRtcpPacket(const uint8_t* data, size_t size)
{
	if (size >= 12 && (data[0] & 0x80) && (data[1] >= 192 && data[1] <= 223)) {
		return true;
	}
	return false;
}
