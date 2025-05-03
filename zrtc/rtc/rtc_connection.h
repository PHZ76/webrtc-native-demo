#pragma once

#include "dtls_connection.h"
#include "udp_connection.h"
#include "rtc_common.h"
#include "rtc_sdp.h"
#include "srtp_session.h"
#include "rtp_source.h"
#include "rtcp_source.h"
#include "rtcp_sink.h"
#include "stun_source.h"
#include "stun_sink.h"

class RtcConnection : public UdpConnection
{
public:
	RtcConnection(std::shared_ptr<xop::EventLoop> event_loop);
	virtual ~RtcConnection();

	bool Init(RtcRole role);
	void Destroy();

	bool SendVideoFrame(uint8_t* frame, size_t frame_size);
	bool SendAudioFrame(uint8_t* frame, size_t frame_size);

	void SetStreamName(std::string stream_name);
	bool SetLocalAddress(std::string ip, uint16_t port);

	void SetRemoteSdp(std::string sdp);
	std::string GetLocalSdp();
	std::string GetLocalUfrag();

private:
	virtual void OnRecv(uint8_t* pkt, size_t pkt_size);
	virtual int  OnSend(uint8_t* pkt, size_t pkt_size);
	void OnSendRtpPackets(std::list<RtpPacketPtr> rtp_pkts);
	void OnSendRtcpPackets(std::list<RtcpPacketPtr> rtcp_pkts);
	void OnStunPacket(uint8_t* pkt, size_t size);
	void OnDtlsPacket(uint8_t* pkt, size_t size);
	void OnRtpPacket(uint8_t* pkt, size_t size);
	void OnRtcpPacket(uint8_t* pkt, size_t size);
	void CheckSendRtcp();
	void CheckNack();

	uint32_t audio_ssrc_ = 0;
	uint32_t video_ssrc_ = 0;
	uint32_t rtx_ssrc_ = 0;
	std::unordered_map<uint32_t, std::shared_ptr<RtpSource>> rtp_sources_;
	std::unordered_map<uint32_t, std::shared_ptr<RtcpSource>> rtcp_sources_;
	std::shared_ptr<RtcpSink> rtcp_sink_;

	std::string stream_name_;
	std::string ice_ufrag_;
	std::string ice_pwd_;
	RtcRole role_ = RTC_ROLE_UNDEFINE;
	RtcSdp local_sdp_;
	RtcSdp remote_sdp_;

	uint32_t check_rtcp_timer_id_ = 0;
	uint32_t check_nack_timer_id_ = 0;
	std::shared_ptr<StunSource> stun_source_;
	std::shared_ptr<StunSink> stun_sink_;

	bool is_handshake_done_ = false;
	std::shared_ptr<DtlsConnection> dtls_connection_;
	std::shared_ptr<SrtpSession> srtp_session_;
};

