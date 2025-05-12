#include "rtp_source.h"

static uint32_t GetH264Timestamp()
{
	return static_cast<uint32_t>((std::chrono::time_point_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now()).time_since_epoch().count() + 500) / 1000 * 90); // 90:(clock_rate / 1000)
}

RtpSource::RtpSource(uint32_t ssrc, uint8_t payload_type)
{
	rtp_header_.version = RTP_VERSION;
	rtp_header_.ssrc = ssrc;
	rtp_header_.payload_type = payload_type;

	header_size_ = RTP_HEADER_SIZE;
}

RtpSource::~RtpSource()
{

}

void RtpSource::SetRtx(uint32_t ssrc, uint8_t payload_type)
{
	rtx_ssrc_ = ssrc;
	rtx_payloa_type_ = payload_type;
	rtp_cache_.resize(RTC_NACK_MAX_RTP_CACHE);
}

void RtpSource::SetFec(uint32_t fec_ssrc, uint8_t payload_type)
{
	fec_ssrc_ = fec_ssrc;
	fec_payload_type_ = payload_type;
	fec_encoder_ = std::make_shared<FecEncoder>(rtp_header_.ssrc, fec_ssrc, fec_payload_type_);
}

void RtpSource::SetExtension(RtpExtensionType ext_type)
{
	if (extension_pos_.count(ext_type)) {
		return;
	}

	if (extension_size_ == 0) {
		extension_size_ = RTX_EXTENSION_HEADER_SIZE;
	}

	switch (ext_type)
	{
	case RTP_EXTENSION_TWCC:
		extension_size_ += 4;
		extension_pos_[RTP_EXTENSION_TWCC] = 0;
		break;
	default:
		break;
	}

	rtp_header_.extension = 1;
	header_size_ = RTP_HEADER_SIZE + extension_size_;
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

	// one byte header
	if (rtp_header_.extension) {
		rtp_header[12] = 0xbe;
		rtp_header[13] = 0xde;
		WriteUint16BE(&rtp_header[14], (extension_size_ - 4) / 4);
	}

	uint32_t ext_pos = RTP_HEADER_SIZE + RTX_EXTENSION_HEADER_SIZE;
	for (auto& ext : extension_pos_) {
		switch(ext.first) 
		{
		case RTP_EXTENSION_TWCC:
			rtp_header[ext_pos++] = RTP_EXTENSION_TWCC << 4 | 1;
			ext.second = ext_pos;
			rtp_header[ext_pos++] = 0; // seq
			rtp_header[ext_pos++] = 0; // seq
			rtp_header[ext_pos++] = 0; // pad
			break;
		default:
			break;
		}
	}

	rtp_pkt->ssrc = rtp_header_.ssrc;
	rtp_pkt->sequence = rtp_header_.sequence;
	rtp_pkt->timestamp = rtp_header_.timestamp;
	rtp_pkt->marker = rtp_header_.marker;
}

void RtpSource::UpdateRtpCache(std::list<RtpPacketPtr>& rtp_pkts)
{
	if (rtx_ssrc_ == 0) {
		return;
	}

	for (auto pkt : rtp_pkts) {
		if (!pkt->is_rtx_) {
			rtp_cache_[pkt->sequence % RTC_NACK_MAX_RTP_CACHE] = pkt;
		}
	}
}

void RtpSource::UpdateExtSequence(RtpPacketPtr& rtp_packet, uint16_t conn_seq)
{
	if (extension_pos_.count(RTP_EXTENSION_TWCC)) {
		uint32_t ext_pos = extension_pos_[RTP_EXTENSION_TWCC];
		if (ext_pos != 0) {
			WriteUint16BE(rtp_packet->data.get() + ext_pos, conn_seq);
		}
	}
}

void RtpSource::UpdateQoS(uint32_t rtt, uint32_t loss_rate)
{
	rtt_ = rtt;
	smooth_rtt_ = (smooth_rtt_ * 3 + rtt_ * 1) >> 2; // 3/4 + 1/4
	loss_rate_ = loss_rate;

	if (fec_encoder_) {
		fec_encoder_->UpdateLossRate(loss_rate);
	}
}

void RtpSource::RetransmitRtpPackets(std::vector<uint16_t>& lost_seqs)
{
	if (rtx_ssrc_ == 0) {
		return;
	}

	std::list<RtpPacketPtr> rtx_pkts;
	for (auto lost_seq :  lost_seqs) {
		auto rtp_packet = rtp_cache_[lost_seq % RTC_NACK_MAX_RTP_CACHE];
		if (rtp_packet) {
			auto rtx_packet = std::make_shared<RtpPacket>();
			BuildHeader(rtx_packet);
			uint8_t* rtx_header = rtx_packet->data.get();
			rtx_header[1] = rtp_packet->marker << 7 | rtx_payloa_type_;
			WriteUint16BE(&rtx_header[2], rtx_seq_++);
			WriteUint32BE(&rtx_header[4], rtp_packet->timestamp);
			WriteUint32BE(&rtx_header[8], rtx_ssrc_);

			WriteUint16BE(&rtx_header[header_size_], rtp_packet->sequence);
			memcpy(rtx_packet->data.get() + header_size_ + sizeof(rtp_packet->sequence),
				rtp_packet->data.get() + header_size_, rtp_packet->data_size - header_size_);
			rtx_packet->data_size =  rtp_packet->data_size + sizeof(rtp_packet->sequence);
			rtx_packet->ssrc = rtp_header_.ssrc;
			rtx_packet->is_rtx_ = 1;
			rtx_pkts.push_back(rtx_packet);
			// RTC_LOG_INFO("retrans rtp:{} --> rtx:{}", rtp_packet->sequence, rtx_seq_ - 1);
		}
	}

	if (!rtx_pkts.empty() && send_pkt_callback_) {
		send_pkt_callback_(rtx_pkts);
	}
}

void RtpSource::GeneratedFecPacket(std::list<RtpPacketPtr>& rtp_pkts)
{
	if (fec_ssrc_ == 0) {
		return;
	}

	std::list<RtpPacketPtr> rtp_fec_pkts;
	for (auto rtp_pkt : rtp_pkts) {
		fec_encoder_->AddRtpPacket(rtp_pkt);

		std::list<webrtc::ForwardErrorCorrection::Packet*> fec_packets;
		fec_encoder_->GetFecPackets(fec_packets);

		if (!fec_packets.empty()) {
			for (const auto* fec_pkt : fec_packets) {
				auto rtp_fec_packet = std::make_shared<RtpPacket>();
				BuildHeader(rtp_fec_packet);
				rtp_fec_packet->ssrc = rtp_header_.ssrc;
				rtp_fec_packet->is_fec_ = 1;
				rtp_fec_packet->marker = 0;

				uint8_t* rtp_header = rtp_fec_packet->data.get();
				rtp_header[1] = rtp_fec_packet->marker << 7 | fec_payload_type_;
				WriteUint16BE(&rtp_header[2], fec_seq_++);
				WriteUint32BE(&rtp_header[4], GetH264Timestamp());
				WriteUint32BE(&rtp_header[8], fec_ssrc_);

				memcpy(rtp_fec_packet->data.get() + header_size_, fec_pkt->data.cdata(), fec_pkt->data.size());
				rtp_fec_packet->data_size = header_size_ + static_cast<uint32_t>(fec_pkt->data.size());
				rtp_fec_pkts.push_back(rtp_fec_packet);
			}
		}
	}

	if (!rtp_fec_pkts.empty()) {
		rtp_pkts.insert(rtp_pkts.end(), rtp_fec_pkts.begin(), rtp_fec_pkts.end());
	}
}