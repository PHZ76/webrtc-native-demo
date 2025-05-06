#include "fec_encoder.h"

static uint32_t GetH264Timestamp()
{
	return static_cast<uint32_t>((std::chrono::time_point_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now()).time_since_epoch().count() + 500) / 1000 * 90); // 90:(clock_rate / 1000)
}

FecEncoder::FecEncoder(uint32_t media_ssrc, uint32_t fec_ssrc, uint32_t fec_payload_type)
	: media_ssrc_(media_ssrc)
	, fec_ssrc_(fec_ssrc)
	, fec_payload_type_(fec_payload_type)
{
	fec_ = webrtc::ForwardErrorCorrection::CreateFlexfec(fec_ssrc, media_ssrc);
}

FecEncoder::~FecEncoder()
{

}

void FecEncoder::UpdateLossRate(uint32_t loss_rate)
{
	float alpha = 0.1f;
	loss_rate_ = loss_rate;
	smoothed_loss_rate_ = static_cast<uint32_t>(alpha * loss_rate + (1 - alpha) * smoothed_loss_rate_);
}

void FecEncoder::AddRtpPacket(std::shared_ptr<RtpPacket> rtp_packet)
{
	// 单帧最大支持的包数, 超过则不生成冗余
	if (media_packets_.size() < webrtc::kUlpfecMaxMediaPackets) {
		auto fec_packet = std::make_unique<webrtc::ForwardErrorCorrection::Packet>();
		rtc::CopyOnWriteBuffer packet(rtp_packet->data.get(), rtp_packet->data_size);
		fec_packet->data = std::move(packet);
		media_packets_.push_back(std::move(fec_packet));
	}

	const size_t min_media_packets = 8;
	const int num_important_packets = 0;
	const bool use_unequal_protection = false;
	const bool complete_frame = rtp_packet->marker == 1 ? true : false;

	// 不支持跨帧打冗余
	if (complete_frame || media_packets_.size() >= min_media_packets) {
		uint8_t protection_factor = smoothed_loss_rate_ * 255 / 100;
		fec_->EncodeFec(media_packets_, protection_factor, num_important_packets,
			use_unequal_protection, webrtc::FecMaskType::kFecMaskRandom, &fec_packets_);

		if (fec_packets_.empty()) {
			media_packets_.clear();
		}
	}
}

std::list<std::shared_ptr<RtpPacket>> FecEncoder::GetFecPackets()
{
	std::list<std::shared_ptr<RtpPacket>> fec_packets;

	for (const auto* fec_pkt : fec_packets_) {
		auto rtp_fec_packet = std::make_shared<RtpPacket>();
		rtp_fec_packet->ssrc = fec_ssrc_;
		rtp_fec_packet->is_fec_ = 1;
		rtp_fec_packet->marker = 0;

		uint8_t* rtp_header = rtp_fec_packet->data.get();
		rtp_header[0] |= RTP_VERSION << 6;
		rtp_header[1] = rtp_fec_packet->marker << 7 | fec_payload_type_;
		WriteUint16BE(&rtp_header[2], fec_seq_++);
		WriteUint32BE(&rtp_header[4], GetH264Timestamp());
		WriteUint32BE(&rtp_header[8], fec_ssrc_);

		memcpy(rtp_fec_packet->data.get() + RTP_HEADER_SIZE, fec_pkt->data.cdata(), fec_pkt->data.size());
		rtp_fec_packet->data_size = RTP_HEADER_SIZE + static_cast<uint32_t>(fec_pkt->data.size());
		fec_packets.push_back(rtp_fec_packet);
	}

	if (!fec_packets_.empty()) {
		fec_packets_.clear();
		media_packets_.clear();
	}
	return fec_packets;
}
