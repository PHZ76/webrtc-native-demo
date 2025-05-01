#pragma once

#include <cstdint>
#include <cstdint>
#include <memory>
#include <vector>

#define RTCP_HEADER_OPTION_VERSION            0x01
#define RTCP_HEADER_OPTION_PADDING            0x02
#define RTCP_HEADER_OPTION_RC                 0x03
#define RTCP_HEADER_OPTION_PAYLOAD_TYPE       0x04
#define RTCP_HEADER_OPTION_LENGTH             0x05

#define RTCP_HEADER_SIZE 4

#define RTCP_SENDER_REPORT_PT    200
#define RTCP_SENDER_REPORT_SIZE  28

struct RtcpHeader
{
	uint8_t  version      : 2;
	uint8_t  padding      : 1;
	uint8_t  rc           : 5;
	uint8_t  payload_type;
	uint16_t length ;

	void SetOption(int opt, int value)
	{
		switch (opt)
		{
		case RTCP_HEADER_OPTION_VERSION:
			version = value;
			break;
		case RTCP_HEADER_OPTION_PADDING:
			padding = value ? 1 : 0;
			break;
		case RTCP_HEADER_OPTION_RC:
			rc = value;
			break;
		case RTCP_HEADER_OPTION_PAYLOAD_TYPE:
			payload_type = value;
			break;
		case RTCP_HEADER_OPTION_LENGTH:
			length = value;
			break;
		default:
			break;
		}
	}

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
