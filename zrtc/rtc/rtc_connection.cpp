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

	RTC_LOG_INFO("create peer connection, video_ssrc:{} audio_ssrc:{}", video_ssrc_, audio_ssrc_);
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

	local_sdp_.SetAddress(local_ip_, local_port_);
	local_sdp_.SetIceParams(ice_ufrag_, ice_pwd_);
	local_sdp_.SetFingerprint(dtls_connection_->GetFingerprint());
	local_sdp_.SetStreamName(stream_name_);

	local_sdp_.SetAudioSsrc(audio_ssrc_);
	local_sdp_.SetAudioPayloadType(RTC_MEDIA_CODEC_OPUS);
	rtp_sources_[audio_ssrc_] = std::make_shared<OpusRtpSource>(audio_ssrc_, RTC_MEDIA_CODEC_OPUS);
	rtp_sources_[audio_ssrc_]->SetSendPacketCallback([this](std::list<RtpPacketPtr> rtp_pkts) {
		OnSendRtpPackets(rtp_pkts);
	});

	local_sdp_.SetVideoSsrc(video_ssrc_);
	local_sdp_.SetVideoPayloadType(RTC_MEDIA_CODEC_H264);
	rtp_sources_[video_ssrc_] = std::make_shared<H264RtpSource>(video_ssrc_, RTC_MEDIA_CODEC_H264);
	rtp_sources_[video_ssrc_]->SetSendPacketCallback([this](std::list<RtpPacketPtr> rtp_pkts) {
		OnSendRtpPackets(rtp_pkts);
	});

	return true;
}

void RtcConnection::Destroy()
{
	UdpConnection::Destroy();
	is_handshake_done_ = false;
	rtp_sources_.clear();
	dtls_connection_->Destroy();
}

bool RtcConnection::SendVideoFrame(uint8_t* frame, size_t frame_size)
{
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
				int rtp_pkt_size = srtp_session_->ProtectRtp(pkt->data.get(), pkt->data_size);
				//RTC_LOG_INFO("ProtectRtp:{} ==> {}", pkt->data_size, rtp_pkt_size);
				if (rtp_pkt_size > 0) {
					OnSend(pkt->data.get(), rtp_pkt_size);
				}
			}
		}
	});
}

void RtcConnection::OnStunPacket(uint8_t* stun_pkt, size_t size)
{
	bool result = stun_receiver_.Parse(stun_pkt, size);
	if (result) {
		stun_sender_.SetMessageType(STUN_BINDING_RESPONSE);
		stun_sender_.SetTransactionId(stun_receiver_.GetTransactionId());
		stun_sender_.SetUserame(remote_sdp_.GetIceUfrag() + ":" + ice_ufrag_);
		stun_sender_.SetPassword(ice_pwd_);
		stun_sender_.SetMappedAddress(ntohl(peer_addr_.sin_addr.s_addr));
		stun_sender_.SetMappedPort(ntohs(peer_addr_.sin_port));
		auto stun_response = stun_sender_.Build();
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
	//RTC_LOG_INFO("recv rtp:{}", size);
}

void RtcConnection::OnRtcpPacket(uint8_t* pkt, size_t size)
{
	//RTC_LOG_INFO("recv rtcp:{}", size);
	int rtcp_pkt_size = srtp_session_->UnprotectRtcp(pkt, (int)size);
	//RTC_LOG_INFO("unprotect rtcp:{}", rtcp_pkt_size);
}