#include "fec_encoder.h"

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

void FecEncoder::GetFecPackets(std::list<webrtc::ForwardErrorCorrection::Packet*>& fec_packets)
{
	fec_packets.swap(fec_packets_);
	if (!fec_packets.empty()) {
		fec_packets_.clear();
		media_packets_.clear();
	}
}
