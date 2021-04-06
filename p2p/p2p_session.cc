#include "p2p_session.h"
#include "api/create_peerconnection_factory.h"
#include "rtc_base/strings/json.h"
#include "rtc_log.h"

class DummySetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver 
{
public:
	static DummySetSessionDescriptionObserver* Create() 
	{
		return new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
	}

	virtual void OnSuccess() 
	{ 
		
	}

	virtual void OnFailure(webrtc::RTCError error) 
	{
		LOG_INFO("%s : %s", ToString(error.type()), error.message());
	}
};

P2PSession::P2PSession(webrtc::PeerConnectionFactoryInterface* peer_connection_factory)
	: peer_connection_factory_(peer_connection_factory)
{

}

P2PSession::~P2PSession()
{
	OnClose();
}

void P2PSession::SetWriteCallback(const WriteCallback& callback)
{
	write_callback_ = callback;
}

bool P2PSession::OnRead(std::string message)
{
	if (!peer_connection_) {
		if (!CreateConnection()) {
			return false;
		}

		AddTracks();
	}

	if (!peer_connection_) {
		return false;
	}

	Json::Reader reader;
	Json::Value jmessage;
	if (!reader.parse(message, jmessage)) {
		LOG_INFO("Received unknown message: %s", message.c_str());
		return false;
	}

	std::string msg_type;
	std::string sdp;
	rtc::GetStringFromJsonObject(jmessage, "type", &msg_type);

	if (msg_type == "offer") {
		rtc::GetStringFromJsonObject(jmessage, "sdp", &sdp);
		SetOffer(sdp);
		SendAnswer();
	}
	else if (msg_type == "answer") {
		rtc::GetStringFromJsonObject(jmessage, "sdp", &sdp);
		SetAnswer(sdp);
	}
	else if (msg_type == "ice_candidate") {
		std::string sdp_mid;
		int sdp_mline_index = 0;
		std::string sdp;

		if (!rtc::GetStringFromJsonObject(jmessage, "sdp_mid", &sdp_mid) ||
			!rtc::GetIntFromJsonObject(jmessage, "sdp_mline_index", &sdp_mline_index) ||
			!rtc::GetStringFromJsonObject(jmessage, "candidate", &sdp)) {
			LOG_INFO("Can't parse received message.: %s", message.c_str());
			return false;
		}
		SetIceCandidate(sdp_mid, sdp_mline_index, sdp);
	}

	return true;
}

void P2PSession::OnWrite(std::string message)
{
	if (write_callback_) {
		write_callback_(message);
	}
}

void P2PSession::OnClose()
{
	audio_track_ = nullptr;
	video_track_ = nullptr;
	peer_connection_ = nullptr;
}

bool P2PSession::AddAudioSource(rtc::scoped_refptr<webrtc::AudioSourceInterface> audio_source)
{
	audio_source_ = audio_source;
	return true;
}

bool P2PSession::AddVideoSource(rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source)
{
	video_source_ = video_source;
	return true;
}

bool P2PSession::AddVideoRenderer(std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> video_renderer)
{
	video_renderer_ = video_renderer;
	return true;
}

bool P2PSession::SendOffer()
{
	if (peer_connection_) {
		return false;
	}

	if (!CreateConnection()) {
		return false;
	}

	if (!AddTracks()) {

	}

	auto options = webrtc::PeerConnectionInterface::RTCOfferAnswerOptions();
	options.offer_to_receive_video = 1;
	options.offer_to_receive_audio = 1;
	peer_connection_->CreateOffer(this, options);
	return true;
}

bool P2PSession::SendAnswer()
{
	if (!peer_connection_) {
		return false;
	}

	auto options = webrtc::PeerConnectionInterface::RTCOfferAnswerOptions();
	peer_connection_->CreateAnswer(this, options);
	return true;
}

bool P2PSession::SetOffer(std::string sdp)
{
	if (!peer_connection_) {
		return false;
	}

	webrtc::SdpParseError error;
	auto session_description = webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, sdp, &error);
	if (!session_description) {
		LOG_INFO("Can't parse received offer, %s ", error.description);
		return false;
	}

	peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), session_description.release());
	return true;
}

bool P2PSession::SetAnswer(std::string sdp)
{
	if (!peer_connection_) {
		return false;
	}

	webrtc::SdpParseError error;
	auto session_description = webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp, &error);
	if (!session_description) {
		LOG_INFO("Can't parse received answer, %s ", error.description);
		return false;
	}

	peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), session_description.release());
	return true;
}

bool P2PSession::SetIceCandidate(std::string sdp_mid, int sdp_mline_index, std::string sdp)
{
	if (!peer_connection_) {
		return false;
	}

	webrtc::SdpParseError error;
	std::unique_ptr<webrtc::IceCandidateInterface> candidate(webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, sdp, &error));
	if (!candidate.get()) {
		LOG_INFO("Can't parse received candidate, %s ", error.description);
		return false;
	}

	if (!peer_connection_->AddIceCandidate(candidate.get())) {
		LOG_INFO("Add ice candidate failed.");
		return false;
	}

	return true;
}

bool P2PSession::CreateConnection()
{
	webrtc::PeerConnectionInterface::RTCConfiguration config;
	config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
	config.enable_dtls_srtp = true;
	webrtc::PeerConnectionInterface::IceServer server;
	server.uri = "stun:stun.l.google.com:19302";
	config.servers.push_back(server);

	peer_connection_ = peer_connection_factory_->CreatePeerConnection(config, nullptr, nullptr, this);
	return peer_connection_ != nullptr;
}

bool P2PSession::AddTracks()
{
	if (!peer_connection_->GetSenders().empty()) {
		return false;  // Already added tracks.
	}

	if (!audio_track_  && audio_source_) {
		audio_track_ = peer_connection_factory_->CreateAudioTrack("audio_label", audio_source_);
	}

	if (!video_track_ && video_source_) {
		video_track_ = peer_connection_factory_->CreateVideoTrack("video_label", video_source_);
	}

	if (audio_track_) {
		auto result_or_error = peer_connection_->AddTrack(audio_track_, { "stream_id" });
		if (!result_or_error.ok()) {
			LOG_ERROR("Failed to add audio track to PeerConnection: %s", result_or_error.error().message());
		}
	}

	if (video_track_) {
		auto result_or_error = peer_connection_->AddTrack(video_track_, { "stream_id" });
		if (!result_or_error.ok()) {
			LOG_ERROR("Failed to add video track to PeerConnection: %s", result_or_error.error().message());
		}
	}

	return true;
}

void P2PSession::OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
							const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams)
{
	auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(receiver->track().release());
	if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
		if (video_renderer_) {
			webrtc::VideoTrackInterface* video_track = static_cast<webrtc::VideoTrackInterface*>(track);
			video_track->AddOrUpdateSink(video_renderer_.get(), rtc::VideoSinkWants());
		}
	}
	track->Release();
}

void P2PSession::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{

}

void P2PSession::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{
	Json::StyledWriter writer;
	Json::Value jmessage;
	jmessage["type"] = "ice_candidate";
	jmessage["sdp_mid"] = candidate->sdp_mid();
	jmessage["sdp_mline_index"] = candidate->sdp_mline_index();

	std::string sdp;
	if (!candidate->ToString(&sdp)) {
		RTC_LOG(LS_ERROR) << "Failed to serialize candidate";
		return;
	}
	LOG_INFO("candidate: %s", sdp.c_str());
	jmessage["candidate"] = sdp;
	OnWrite(writer.write(jmessage));
}

void P2PSession::OnSuccess(webrtc::SessionDescriptionInterface* desc)
{
	peer_connection_->SetLocalDescription(DummySetSessionDescriptionObserver::Create(), desc);

	std::string sdp;
	desc->ToString(&sdp);

	Json::StyledWriter writer;
	Json::Value jmessage;
	jmessage["type"] = webrtc::SdpTypeToString(desc->GetType()); 
	jmessage["sdp"] = sdp;
	LOG_INFO("sdp: %s", sdp.c_str());
	OnWrite(writer.write(jmessage));
}

void P2PSession::OnFailure(webrtc::RTCError error)
{
	LOG_ERROR("%s : %s", ToString(error.type()), error.message());
}
