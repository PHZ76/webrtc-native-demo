#include "opus_rtp_source.h"
#include "rtc_common.h"

OpusRtpSource::OpusRtpSource(uint32_t ssrc, uint32_t payload_type)
	: RtpSource(ssrc, payload_type)
{
    clock_rate_ = 48000;
}

OpusRtpSource::~OpusRtpSource()
{

}

void OpusRtpSource::InputFrame(uint8_t* frame_data, size_t frame_size)
{
    std::list<RtpPacketPtr> rtp_pkts;
    timestamp_ += 480;

    std::shared_ptr<RtpPacket> rtp_pkt(new RtpPacket());
    SetTimestamp(timestamp_);
    SetMarker(1);
    SetSequence(sequence_++);
    BuildHeader(rtp_pkt);
    memcpy(rtp_pkt->data.get() + RTP_HEADER_SIZE, frame_data, frame_size);
    rtp_pkt->data_size = RTP_HEADER_SIZE + static_cast<uint32_t>(frame_size);
    rtp_pkts.push_back(rtp_pkt);

    if (rtp_pkts.size() > 0 && send_pkt_callback_) {
        send_pkt_callback_(rtp_pkts);
    }
}