#pragma once

#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include <string>
#include <thread>
#include <functional>

class P2PSession : public webrtc::PeerConnectionObserver,
				   public webrtc::CreateSessionDescriptionObserver
{
public:
	using WriteCallback = std::function<void(std::string)>;

	P2PSession& operator=(const P2PSession&) = delete;
	P2PSession(const P2PSession&) = delete;
	P2PSession(webrtc::PeerConnectionFactoryInterface* peer_connection_factory);
	virtual ~P2PSession();

	void SetWriteCallback(const WriteCallback& callback);

	bool OnRead(std::string message);
	void OnClose();

	bool AddAudioSource(rtc::scoped_refptr<webrtc::AudioSourceInterface> audio_source);
	bool AddVideoSource(rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source);
	bool AddVideoRenderer(std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> video_renderer);
	bool SendOffer();
	
protected:
	void OnWrite(std::string message);
	bool SendAnswer();
	bool SetOffer(std::string sdp);
	bool SetAnswer(std::string sdp);
	bool SetIceCandidate(std::string sdp_mid, int sdp_mline_index, std::string candidate);

	// PeerConnectionObserver implementation.
	void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override {}
	void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
					const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override;
	void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
	void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}
	void OnRenegotiationNeeded() override {}
	void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override {}
	void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}
	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
	void OnIceConnectionReceivingChange(bool receiving) override {}

	// CreateSessionDescriptionObserver implementation.
	void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
	void OnFailure(webrtc::RTCError error) override;

	bool CreateConnection();
	bool AddTracks();

	WriteCallback write_callback_;
	webrtc::PeerConnectionFactoryInterface* peer_connection_factory_ = nullptr;
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

	bool enable_audio_ = false;
	rtc::scoped_refptr<webrtc::AudioTrackInterface>         audio_track_;
	rtc::scoped_refptr<webrtc::VideoTrackInterface>         video_track_;
	rtc::scoped_refptr<webrtc::AudioSourceInterface>        audio_source_;
	rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>   video_source_;

	std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> video_renderer_;
};
