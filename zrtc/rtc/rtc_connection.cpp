#include "rtc_connection.h"
#include "rtc_log.h"
#include "rtc_utils.h"
#include "opus_rtp_source.h"
#include "h264_rtp_source.h"

RtcConnection::RtcConnection(std::shared_ptr<xop::EventLoop> event_loop)
	: UdpConnection(event_loop)
{
	ice_ufrag_ = GenerateRandomString(RTC_ICE_UFRAG_LENGTH);
	ice_pwd_ = GenerateRandomString(RTC_ICE_PASSWORD_LENGTH);

	video_ssrc_ = GenerateSSRC();
	audio_ssrc_ = GenerateSSRC();
	rtx_ssrc_ = GenerateSSRC();

	RTC_LOG_INFO("create peer connection, video_ssrc:{} audio_ssrc:{} video_rtx_ssrc:{}", video_ssrc_, audio_ssrc_, rtx_ssrc_);
}

RtcConnection::~RtcConnection()
{
	Destroy();
}

void RtcConnection::SetStreamName(std::string stream_name)
{
	stream_name_ = stream_name;
}

bool RtcConnection::SetLocalAddress(std::string ip, uint16_t port)
{
	local_port_ = port;
	local_ip_ = ip;

	if (!UdpConnection::Init(local_ip_, local_port_)) {
		RTC_LOG_ERROR("create socket failed, port:{}", local_port_);
		return false;
	}

	return true;
}

bool RtcConnection::Init(RtcRole role)
{
	role_ = role;
	dtls_connection_ = std::make_shared<DtlsConnection>();
	dtls_connection_->Init(role_);

	stun_source_ = std::make_shared<StunSource>();
	stun_sink_ = std::make_shared<StunSink>();

	local_sdp_.SetAddress(local_ip_, local_port_);
	local_sdp_.SetIceParams(ice_ufrag_, ice_pwd_);
	local_sdp_.SetFingerprint(dtls_connection_->GetFingerprint());
	local_sdp_.SetStreamName(stream_name_);

	local_sdp_.SetAudioSsrc(audio_ssrc_);
	local_sdp_.SetAudioPayloadType(RTC_MEDIA_CODEC_OPUS);
	rtcp_sources_[audio_ssrc_] = std::make_shared<RtcpSource>(audio_ssrc_);
	rtcp_sources_[audio_ssrc_]->SetSenderReportInterval(5000);
	rtp_sources_[audio_ssrc_] = std::make_shared<OpusRtpSource>(audio_ssrc_, RTC_MEDIA_CODEC_OPUS);
	rtp_sources_[audio_ssrc_]->SetSendPacketCallback([this](std::list<RtpPacketPtr> rtp_pkts) {
		OnSendRtpPackets(rtp_pkts);
	});

	local_sdp_.SetVideoSsrc(video_ssrc_, rtx_ssrc_);
	local_sdp_.SetVideoPayloadType(RTC_MEDIA_CODEC_H264, RTC_MEDIA_CODEC_RTX);
	rtcp_sources_[video_ssrc_] = std::make_shared<RtcpSource>(video_ssrc_);
	rtcp_sources_[video_ssrc_]->SetSenderReportInterval(1000);
	rtp_sources_[video_ssrc_] = std::make_shared<H264RtpSource>(video_ssrc_, RTC_MEDIA_CODEC_H264);
	rtp_sources_[video_ssrc_]->SetRtx(rtx_ssrc_, RTC_MEDIA_CODEC_RTX);
	rtp_sources_[video_ssrc_]->SetSendPacketCallback([this](std::list<RtpPacketPtr> rtp_pkts) {
		OnSendRtpPackets(rtp_pkts);
	});

	rtcp_sink_ = std::make_shared<RtcpSink>();

	check_rtcp_timer_id_ = event_loop_->AddTimer([this]() {
		CheckSendRtcp();
		return true;
	}, 1000);

	check_nack_timer_id_ = event_loop_->AddTimer([this]() {
		CheckNack();
		return true;
	}, 5);

	return true;
}

void RtcConnection::Destroy()
{
	event_loop_->RemoveTimer(check_rtcp_timer_id_);
	event_loop_->RemoveTimer(check_nack_timer_id_);
	check_rtcp_timer_id_ = 0;
	check_nack_timer_id_ = 0;

	is_handshake_done_ = false;
	dtls_connection_->Destroy();

	rtp_sources_.clear();
	rtcp_sources_.clear();
	rtcp_sink_ = nullptr;

	UdpConnection::Destroy();
}

bool RtcConnection::SendVideoFrame(uint8_t* frame, size_t frame_size)
{
	if (!is_handshake_done_) {
		return false;
	}

	if (rtp_sources_.count(video_ssrc_)) {
		auto rtp_source = std::dynamic_pointer_cast<H264RtpSource>(rtp_sources_[video_ssrc_]);
		if (rtp_source) {
			rtp_source->InputFrame(frame, frame_size);
		}
	}

	return true;
}

bool RtcConnection::SendAudioFrame(uint8_t* frame, size_t frame_size)
{
	if (!is_handshake_done_) {
		return false;
	}

	if (rtp_sources_.count(audio_ssrc_)) {
		auto rtp_source = std::dynamic_pointer_cast<OpusRtpSource>(rtp_sources_[audio_ssrc_]);
		if (rtp_source) {
			rtp_source->InputFrame(frame, frame_size);
		}
	}
	return true;
}

void RtcConnection::SetRemoteSdp(std::string sdp)
{
	remote_sdp_.Parse(sdp);

	if (role_ == RTC_ROLE_SERVER) {
		dtls_connection_->Listen();
	}
	else if (role_ == RTC_ROLE_CLIENT) {
		dtls_connection_->Connect();
	}
}

std::string RtcConnection::GetLocalSdp()
{
	return local_sdp_.Build();
}

std::string RtcConnection::GetLocalUfrag()
{
	return local_sdp_.GetIceUfrag();
}

void RtcConnection::OnRecv(uint8_t* pkt, size_t pkt_size)
{
	if (pkt_size > 0) {
		if (IsRtpPacket(pkt, pkt_size)) {
			OnRtpPacket(pkt, pkt_size);
		}
		else if (IsRtcpPacket(pkt, pkt_size)) {
			OnRtcpPacket(pkt, pkt_size);
		}
		else if (IsStunPacket(pkt, pkt_size)) {
			OnStunPacket(pkt, pkt_size);
		}
		else if (IsDtlsPacket(pkt, pkt_size)) {
			OnDtlsPacket(pkt, pkt_size);
		}
	}
}

int RtcConnection::OnSend(uint8_t* pkt, size_t pkt_size)
{
	int sent_bytes = UdpConnection::OnSend(pkt, pkt_size);
	if (sent_bytes < 0) {
		RTC_LOG_ERROR("sendto error: {}", strerror(errno));
	}
	return sent_bytes;
}

void RtcConnection::OnSendRtpPackets(std::list<RtpPacketPtr> rtp_pkts)
{
	event_loop_->AddTriggerEvent([this, rtp_pkts] {
		if (!is_handshake_done_) {
			return;
		}
		for (auto pkt : rtp_pkts) {
			if (pkt) {
				uint8_t srtp_buffer[MAX_MTU] = { 0 };
				memcpy(srtp_buffer, pkt->data.get(), pkt->data_size);

				int rtp_pkt_size = srtp_session_->ProtectRtp(srtp_buffer, pkt->data_size);
				if (rtp_pkt_size > 0) {
					OnSend(srtp_buffer, rtp_pkt_size);

					// update rtcp stats
					if (!pkt->is_rtx_ && rtcp_sources_.count(pkt->ssrc)) {
						auto rtcp_source = rtcp_sources_[pkt->ssrc];
						rtcp_source->OnSendRtp(pkt->data_size, pkt->timestamp);
					}
				}
			}
		}
	});
}

void RtcConnection::OnSendRtcpPackets(std::list<RtcpPacketPtr> rtcp_pkts)
{
	event_loop_->AddTriggerEvent([this, rtcp_pkts] {
		if (!is_handshake_done_) {
			return;
		}

		for (auto pkt : rtcp_pkts) {
			if (pkt) {
				int rtcp_pkt_size = srtp_session_->ProtectRtcp(pkt->data.get(), pkt->data_size);
				if (rtcp_pkt_size > 0) {
					OnSend(pkt->data.get(), rtcp_pkt_size);
				}
				else {
					break;
				}
			}
		}
	});
}

void RtcConnection::OnStunPacket(uint8_t* stun_pkt, size_t size)
{
	if (!stun_source_ || !stun_sink_) {
		return;
	}

	bool result = stun_sink_->Parse(stun_pkt, size);
	if (result) {
		stun_source_->SetMessageType(STUN_BINDING_RESPONSE);
		stun_source_->SetTransactionId(stun_sink_->GetTransactionId());
		stun_source_->SetUserame(remote_sdp_.GetIceUfrag() + ":" + ice_ufrag_);
		stun_source_->SetPassword(ice_pwd_);
		stun_source_->SetMappedAddress(ntohl(peer_addr_.sin_addr.s_addr));
		stun_source_->SetMappedPort(ntohs(peer_addr_.sin_port));
		auto stun_response = stun_source_->Build();
		OnSend(stun_response.data(), stun_response.size());
	}
}

void RtcConnection::OnDtlsPacket(uint8_t* dtls_pkt, size_t size)
{   
	if (dtls_connection_ == nullptr) {
		return;
	}

	auto dtls_response = dtls_connection_->OnRecv(dtls_pkt, size);
	if (dtls_response.size() > 0) {
		OnSend(dtls_response.data(), dtls_response.size());
	}

	if (dtls_connection_->IsHandshakeDone()) {
		srtp_session_ = std::make_shared<SrtpSession>();
		std::string send_key = dtls_connection_->GetSrtpSendKey();
		std::string recv_key = dtls_connection_->GetSrtpRecvKey();
		if (send_key.empty()) {
			RTC_LOG_ERROR("srtp send key not found.");
			return;
		}
		if (recv_key.empty()) {
			RTC_LOG_ERROR("srtp recv key not found.");
			return;
		}
		if (!srtp_session_->Init(send_key, recv_key)) {
			RTC_LOG_ERROR("srtp session init failed.");
			return;
		}
		is_handshake_done_ = true;
		RTC_LOG_INFO("srtp send-key:{} recv-key:{}", send_key.size(), recv_key.size());
	}
}

void RtcConnection::OnRtpPacket(uint8_t* pkt, size_t size)
{
	//RTC_LOG_INFO("recv rtp, size:{}", size);
}

void RtcConnection::OnRtcpPacket(uint8_t* pkt, size_t size)
{
	if (!srtp_session_) {
		return;
	}

	int rtcp_pkt_size = srtp_session_->UnprotectRtcp(pkt, (int)size);
	if (rtcp_pkt_size > 0 && rtcp_sink_) {
		if (rtcp_sink_->Parse(pkt, rtcp_pkt_size)) {
			CheckNack();
		}
	}
}

void RtcConnection::CheckSendRtcp()
{
	std::list<RtcpPacketPtr> rtcp_pkts;
	NtpTimestamp ntp_timestamp = GetNtpTimestamp();
	
	for (auto rtcp_source : rtcp_sources_) {
		rtcp_source.second->SetNtpTimestamp(ntp_timestamp.seconds, ntp_timestamp.fractions);
		auto rtcp_packet = rtcp_source.second->BuildSenderReport();
		if (rtcp_packet) {
			rtcp_pkts.push_back(rtcp_packet);
		}
	}
	if (!rtcp_pkts.empty()) {
		rtcp_sink_->OnSenderReportRecord(ComPactNtp(ntp_timestamp));
		OnSendRtcpPackets(rtcp_pkts);
	}
}

void RtcConnection::CheckNack()
{
	for (auto rtp_source : rtp_sources_) {
		std::vector<uint16_t> lost_seqs;
		if (rtcp_sink_->GetLostSeq(rtp_source.first, lost_seqs)) {
			if (!lost_seqs.empty()) {
				rtp_source.second->RetransmitRtpPackets(lost_seqs);
			}
		}
	}
}