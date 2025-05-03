#include "rtp_source.h"

RtpSource::RtpSource(uint32_t ssrc, uint8_t payload_type)
{
	rtp_header_.version = RTP_VERSION;
	rtp_header_.ssrc = ssrc;
	rtp_header_.payload_type = payload_type;
}

RtpSource::~RtpSource()
{

}

void RtpSource::SetRtx(uint32_t ssrc, uint8_t payload_type)
{
	rtx_ssrc_ = ssrc;
	rtx_payloa_type_ = payload_type;
}

void RtpSource::SetTimestamp(uint32_t timestamp)
{
	rtp_header_.timestamp = timestamp;
}

void RtpSource::SetMarker(uint8_t marker)
{
	rtp_header_.marker = marker;
}

void RtpSource::SetSequence(uint32_t sequence)
{
	rtp_header_.sequence = sequence;
}

void RtpSource::SetSendPacketCallback(const SendPacketCallback& callback)
{
	send_pkt_callback_ = callback;
}

uint32_t RtpSource::GetTimestamp()
{
	return rtp_header_.timestamp;
}

uint32_t RtpSource::GetSSRC()
{
	return rtp_header_.ssrc;
}

void RtpSource::BuildHeader(std::shared_ptr<RtpPacket> rtp_pkt)
{
	uint8_t* rtp_header = rtp_pkt->data.get();
	rtp_header[0] |= rtp_header_.version << 6;
	rtp_header[0] |= rtp_header_.padding << 5;
	rtp_header[0] |= rtp_header_.extension << 4;
	rtp_header[0] |= rtp_header_.csrc_count;
	rtp_header[1] = rtp_header_.marker << 7 | rtp_header_.payload_type;
	WriteUint16BE(&rtp_header[2], rtp_header_.sequence);
	WriteUint32BE(&rtp_header[4], rtp_header_.timestamp);
	WriteUint32BE(&rtp_header[8], rtp_header_.ssrc);

	rtp_pkt->ssrc = rtp_header_.ssrc;
	rtp_pkt->timestamp = rtp_header_.timestamp;
}


void RtpSource::RetransmitRtpPackets(std::vector<uint16_t>& lost_seqs)
{

}