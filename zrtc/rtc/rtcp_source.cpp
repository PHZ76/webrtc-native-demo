#include "rtcp_source.h"
#include "rtc_utils.h"

RtcpSource::RtcpSource(uint32_t ssrc)
	: ssrc_(ssrc)
{

}

RtcpSource::~RtcpSource()
{

}

void RtcpSource::SetSenderReportInterval(uint64_t sr_interval)
{
	sr_interval_ = sr_interval;
}

void RtcpSource::SetRtpTimestamp(uint32_t timestamp)
{
	rtp_timestamp_ = timestamp;
}

void RtcpSource::SetNtpTimestamp(uint64_t timestamp)
{
	ntp_timestamp_ = timestamp;
	ntp_mword_ = static_cast<uint32_t>(ntp_timestamp_ >> 32);
	ntp_lword_ = static_cast<uint32_t>(ntp_timestamp_ & 0xffffffff);
}

void RtcpSource::OnSendRtp(uint32_t rtp_packet_size, uint32_t timestamp)
{
	rtp_packet_count_ += 1;
	rtp_octet_count_ += rtp_packet_size;
	rtp_timestamp_ = timestamp;
}

RtcpPacketPtr RtcpSource::BuildSR()
{
	if (!CheckSendSR()) {
		return nullptr;
	}

	auto rtcp_packet = std::make_shared<RtcpPacket>();
	RtcpHeader rtcp_header;
	rtcp_header.SetOption(RTCP_HEADER_OPTION_VERSION, 2);
	rtcp_header.SetOption(RTCP_HEADER_OPTION_PADDING, 0);
	rtcp_header.SetOption(RTCP_HEADER_OPTION_RC, 0);
	rtcp_header.SetOption(RTCP_HEADER_OPTION_PAYLOAD_TYPE, RTCP_SENDER_REPORT_PT);
	rtcp_header.SetOption(RTCP_HEADER_OPTION_LENGTH, (RTCP_SENDER_REPORT_SIZE - RTCP_HEADER_SIZE) / 4);
	std::vector<uint8_t> header = rtcp_header.Build();
	std::copy(header.begin(), header.end(), rtcp_packet->data.get());

	ByteArray payload;
	payload.WriteUint32BE(ssrc_);
	payload.WriteUint32BE(ntp_mword_);
	payload.WriteUint32BE(ntp_lword_);
	payload.WriteUint32BE(rtp_timestamp_);
	payload.WriteUint32BE(rtp_packet_count_);
	payload.WriteUint32BE(rtp_octet_count_);

	memcpy(rtcp_packet->data.get() + header.size(), payload.Data(), payload.Size());
	rtcp_packet->data_size = RTCP_SENDER_REPORT_SIZE;
	return rtcp_packet;
}

bool RtcpSource::CheckSendSR()
{
	if (rtp_timestamp_ == 0) {
		return false;
	}

	uint64_t now_time = GetSysTimestamp();
	if (now_time - last_build_sr_time_ <= sr_interval_) {
		return false;
	}
	last_build_sr_time_ = now_time;
	return true;
}