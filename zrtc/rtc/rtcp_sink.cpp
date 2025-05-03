#include "rtcp_sink.h"
#include "rtc_utils.h"
#include "rtc_log.h"

RtcpSink::RtcpSink()
{

}

RtcpSink::~RtcpSink()
{

}

void RtcpSink::OnSenderReportRecord(uint32_t last_sr)
{
    last_sr_records_.emplace(last_sr, GetSysTimestamp());
    if (last_sr_records_.size() > 20) {
        last_sr_records_.erase(last_sr_records_.begin());
    }
}

void RtcpSink::GetLostSeq(uint32_t ssrc, std::vector<uint16_t> lost_seqs)
{
    if (nack_lost_seqs_.count(ssrc)) {
        lost_seqs.swap(nack_lost_seqs_[ssrc]);
        nack_lost_seqs_.erase(ssrc);
    }
}

bool RtcpSink::Parse(uint8_t* pkt, size_t size)
{
    if (size < RTCP_HEADER_SIZE) {
	    return false;
    }

    rtcp_header_.version = pkt[0] >> 6;
    rtcp_header_.padding = (pkt[0] & 0x20) ? 1 : 0;
    rtcp_header_.rc = pkt[0] & 0x1f;
    rtcp_header_.payload_type = pkt[1];
    rtcp_header_.rc = pkt[0] & 0x1f;
    rtcp_header_.length = ReadU16BE(pkt + 2, size - 2);
    uint8_t* payload = pkt + RTCP_HEADER_SIZE;
    uint32_t pading_size = rtcp_header_.padding ? pkt[size - 1] : 0;
    uint32_t payload_size = rtcp_header_.length * 4 - pading_size;

    switch (rtcp_header_.payload_type) {
    case RTCP_PT_RECEIVER_REPORT:
        if (rtcp_header_.rc > 0) {
            OnReceiverReport(payload, payload_size);
        }
        break;
    case RTCP_PT_RTP_FEEDBACK:
        if (rtcp_header_.rc == RTCP_RC_RTP_FEEDBACK) {
            OnRtpFeedback(payload, payload_size);
        }
        break;
    default:
        break;
    }

	return true;
}

void RtcpSink::OnReceiverReport(uint8_t* payload, size_t size)
{
    uint64_t now_time = GetSysTimestamp();
    uint32_t sender_ssrc = ReadU32BE(payload, size);

    for (int index = 4; index < size; index += RTCP_BLOCK_SIZE) {
        ReportBlock report_block;
        report_block.ssrc = ReadU32BE(payload + index, size);
        report_block.fraction_lost = payload[index + 4];
        report_block.cumulative_packets_lost = (payload[index + 5] << 16) | (payload[index + 6] << 8) | (payload[index + 7]);
        report_block.exetened_highest_sequence_number = ReadU32BE(payload + index + 8, size);
        report_block.jitter = ReadU32BE(payload + index + 12, size);
        report_block.last_sr = ReadU32BE(payload + index + 16, size);
        report_block.delay_since_last_sr = ReadU32BE(payload + index + 20, size);

        if (last_sr_records_.count(report_block.last_sr)) {
            uint64_t delay_since_last_sr_ms = report_block.delay_since_last_sr * 1000 / 65536;
            uint64_t rtt = now_time - last_sr_records_[report_block.last_sr] - delay_since_last_sr_ms;
            //RTC_LOG_INFO("ssrc:{} rtt:{}, lost:{} cumulative_packets_lost:{}", 
            //    report_block.ssrc, rtt, (report_block.fraction_lost * 100 / 255), report_block.cumulative_packets_lost);
        }
    }
}

void RtcpSink::OnRtpFeedback(uint8_t* payload, size_t size)
{
    uint32_t sender_ssrc = ReadU32BE(payload, size);
    uint32_t media_ssrc = ReadU32BE(payload + 4, size);

    for (int index = 8; index < size; index += 4) {
        uint16_t nack_pid = ReadU16BE(payload + index, size);
        uint16_t nack_blp = ReadU16BE(payload + index + 2, size);

        std::vector<uint16_t> lost_seqs;
        lost_seqs.push_back(nack_pid);

        for (int i = 0; i < 16; ++i) {
            if (nack_blp & (1 << (15 - i))) {
                lost_seqs.push_back(nack_pid + 1 + i);
            }
        }

        nack_lost_seqs_.emplace(media_ssrc, lost_seqs);
    }
}