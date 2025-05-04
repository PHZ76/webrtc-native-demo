#include "h264_rtp_source.h"
#include "rtc_log.h"

static uint32_t GetH264Timestamp()
{
    return static_cast<uint32_t>((std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()).time_since_epoch().count() + 500) / 1000 * 90); // 90:(clock_rate / 1000)
}

H264RtpSource::H264RtpSource(uint32_t ssrc, uint32_t payload_type)
	: RtpSource(ssrc, payload_type)
{
    clock_rate_ = RTC_H264_CLOCK_RATE;
}

H264RtpSource::~H264RtpSource()
{

}

void H264RtpSource::InputFrame(uint8_t* frame_data, size_t frame_size)
{
    int start_code_size = 0;
    if (frame_data[0] == 0x00 && frame_data[1] == 0x00 && frame_data[2] == 0x01) {
        start_code_size = 3;
    }
    if (frame_data[0] == 0x00 && frame_data[1] == 0x00 && frame_data[2] == 0x00 && frame_data[3] == 0x01) {
        start_code_size = 4;
    }

    uint8_t nalu_type = frame_data[start_code_size] & 0x1f;
    switch (nalu_type) {
        case RTC_H264_FRAME_TYPE_REF:
            HandleRefFrame(frame_data + start_code_size, frame_size - start_code_size);
            break;
        case RTC_H264_FRAME_TYPE_IDR:
            HandleIDRFrame(frame_data + start_code_size, frame_size - start_code_size);
            break;
        case RTC_H264_FRAME_TYPE_SPS:
            HandleSPSFrame(frame_data + start_code_size, frame_size - start_code_size);
            break;
        case RTC_H264_FRAME_TYPE_PPS:
            HandlePPSFrame(frame_data + start_code_size, frame_size - start_code_size);
            break;
        default:
            break;
    }
}

void H264RtpSource::HandleSPSFrame(uint8_t* frame_data, size_t frame_size)
{
    sps_.reset(new uint8_t[frame_size], std::default_delete<uint8_t[]>());
    sps_size_ = static_cast<uint32_t>(frame_size);
    memcpy(sps_.get(), frame_data, sps_size_);
}

void H264RtpSource::HandlePPSFrame(uint8_t* frame_data, size_t frame_size)
{
    pps_.reset(new uint8_t[frame_size], std::default_delete<uint8_t[]>());
    pps_size_ = static_cast<uint32_t>(frame_size);
    memcpy(pps_.get(), frame_data, pps_size_);
}

void H264RtpSource::HandleIDRFrame(uint8_t* frame_data, size_t frame_size)
{
    std::list<RtpPacketPtr> rtp_pkts;
    max_rtp_payload_size_ = RTC_MAX_RTP_PACKET_LENGTH - RTP_HEADER_SIZE;
    timestamp_ = GetH264Timestamp();
    SetTimestamp(timestamp_);

    // STAP-A sps pps
    BuildRtpSTAPA(frame_data, frame_size, rtp_pkts);

    if (frame_size < max_rtp_payload_size_) {
        BuildRtp(frame_data, frame_size, rtp_pkts);
    }
    else {
        BuildRtpFUA(frame_data, frame_size, rtp_pkts);
    }

    if (rtp_pkts.size() > 0 && send_pkt_callback_) {
        UpdateRtpCache(rtp_pkts);
        send_pkt_callback_(rtp_pkts);
    }
}

void H264RtpSource::HandleRefFrame(uint8_t* frame_data, size_t frame_size)
{
    std::list<RtpPacketPtr> rtp_pkts;
    max_rtp_payload_size_ = RTC_MAX_RTP_PACKET_LENGTH - RTP_HEADER_SIZE;
    timestamp_ = GetH264Timestamp();
    SetTimestamp(timestamp_);

    if (frame_size < max_rtp_payload_size_) {
        BuildRtp(frame_data, frame_size, rtp_pkts);
    }
    else {
        BuildRtpFUA(frame_data, frame_size, rtp_pkts);
    }

    if (rtp_pkts.size() > 0 && send_pkt_callback_) {
        UpdateRtpCache(rtp_pkts);
        send_pkt_callback_(rtp_pkts);
    }
}

void H264RtpSource::BuildRtp(uint8_t* frame_data, size_t frame_size, std::list<RtpPacketPtr>& rtp_pkts)
{
    std::shared_ptr<RtpPacket> rtp_pkt(new RtpPacket());
    SetMarker(1);
    SetSequence(sequence_++);
    BuildHeader(rtp_pkt);
    memcpy(rtp_pkt->data.get() + RTP_HEADER_SIZE, frame_data, frame_size);
    rtp_pkt->data_size = RTP_HEADER_SIZE + static_cast<uint32_t>(frame_size);
    rtp_pkts.push_back(rtp_pkt);
}

void H264RtpSource::BuildRtpSTAPA(uint8_t* frame_data, size_t frame_size, std::list<RtpPacketPtr>& rtp_pkts)
{
    std::shared_ptr<RtpPacket> rtp_pkt(new RtpPacket());
    uint8_t* rtp_data_ = rtp_pkt->data.get();

    SetMarker(0);
    SetSequence(sequence_++);
    BuildHeader(rtp_pkt);
    rtp_data_ += RTP_HEADER_SIZE;
    rtp_data_[0] = sps_.get()[0] & (~0x1f) | 24;
    rtp_data_ += 1;
    WriteUint16BE(rtp_data_, sps_size_);
    rtp_data_ += 2;
    memcpy(rtp_data_, sps_.get(), sps_size_);
    rtp_data_ += sps_size_;

    WriteUint16BE(rtp_data_, pps_size_);
    rtp_data_ += 2;
    memcpy(rtp_data_, pps_.get(), pps_size_);
    rtp_data_ += pps_size_;

    rtp_pkt->data_size = RTP_HEADER_SIZE + sps_size_ + pps_size_ + 5;
    rtp_pkts.push_back(rtp_pkt);
}

void H264RtpSource::BuildRtpFUA(uint8_t* frame_data, size_t frame_size, std::list<RtpPacketPtr>& rtp_pkts)
{
    uint8_t type = frame_data[0] & 0x1f;
    uint8_t nri = (frame_data[0] & 0x60) >> 5;

    uint8_t fu_a_size = 2;
    uint8_t fu_a[2] = { 0 };
    fu_a[0] = (frame_data[0] & 0xe0) | 28;
    fu_a[1] = 0x80 | (frame_data[0] & 0x1f);
    frame_data += 1;
    frame_size -= 1;

    while (frame_size + fu_a_size > max_rtp_payload_size_) {
        std::shared_ptr<RtpPacket> rtp_pkt(new RtpPacket());
        SetMarker(0);
        SetSequence(sequence_++);
        BuildHeader(rtp_pkt);
        rtp_pkt->data.get()[RTP_HEADER_SIZE + 0] = fu_a[0];
        rtp_pkt->data.get()[RTP_HEADER_SIZE + 1] = fu_a[1];
        memcpy(rtp_pkt->data.get() + RTP_HEADER_SIZE + fu_a_size, frame_data, max_rtp_payload_size_ - fu_a_size);
        rtp_pkt->data_size = RTP_HEADER_SIZE + max_rtp_payload_size_;
        rtp_pkts.push_back(rtp_pkt);

        frame_data += max_rtp_payload_size_ - fu_a_size;
        frame_size -= max_rtp_payload_size_ - fu_a_size;
        fu_a[1] &= ~0x80;
    }

    {
        std::shared_ptr<RtpPacket> rtp_pkt(new RtpPacket());
        SetMarker(1);
        SetSequence(sequence_++);
        BuildHeader(rtp_pkt);
        fu_a[1] |= 0x40;
        rtp_pkt->data.get()[RTP_HEADER_SIZE + 0] = fu_a[0];
        rtp_pkt->data.get()[RTP_HEADER_SIZE + 1] = fu_a[1];
        memcpy(rtp_pkt->data.get() + RTP_HEADER_SIZE + fu_a_size, frame_data, frame_size);
        rtp_pkt->data_size = RTP_HEADER_SIZE + fu_a_size + static_cast<uint32_t>(frame_size);
        rtp_pkts.push_back(rtp_pkt);
    }
}
