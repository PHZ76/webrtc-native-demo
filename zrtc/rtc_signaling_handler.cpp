#include "rtc_signaling_handler.h"
#include "rtc/rtc_log.h"

RtcSignalingHandler::RtcSignalingHandler(const SignalingConfig& signaling_config)
	: signaling_config_(signaling_config)
	, event_loop_(std::make_shared<xop::EventLoop>())
{
	event_loop_->Loop();
}

RtcSignalingHandler::~RtcSignalingHandler() {
	event_loop_->Quit();
}

void RtcSignalingHandler::GetLocalDescription(std::string uid, std::string & local_sdp) {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(10000, UINT16_MAX);
	uint16_t port = static_cast<uint16_t>(dis(gen));

	auto rtc_connection = std::make_shared<RtcConnection>(event_loop_);
	rtc_connection->SetStreamName(uid);
	if (!rtc_connection->SetLocalAddress(signaling_config_.host.c_str(), port)) {
		port = static_cast<uint16_t>(dis(gen));
		if (!rtc_connection->SetLocalAddress(signaling_config_.host.c_str(), port)) {
			return;
		}
	}
	rtc_connection->Init(RTC_ROLE_SERVER);
	local_sdp = rtc_connection->GetLocalSdp();

	std::lock_guard<std::mutex> locker(conns_mutex_);
	rtc_conns_[uid] = rtc_connection;
	RTC_LOG_INFO("init rtc connection, address:{}:{}", signaling_config_.host.c_str(), port);
}

void RtcSignalingHandler::OnRemoteDescription(std::string uid, std::string remote_sdp) {
	std::lock_guard<std::mutex> locker(conns_mutex_);
	if (rtc_conns_.count(uid)) {
		rtc_conns_[uid]->SetRemoteSdp(remote_sdp);
	}
}

void RtcSignalingHandler::SendVideoFrame(uint8_t* frame, size_t frame_size)
{
	std::lock_guard<std::mutex> locker(conns_mutex_);
	for (auto conn : rtc_conns_) {
		conn.second->SendVideoFrame(frame, frame_size);
	}
}

void RtcSignalingHandler::SendAudioFrame(uint8_t* frame, size_t frame_size)
{
	std::lock_guard<std::mutex> locker(conns_mutex_);
	for (auto conn : rtc_conns_) {
		conn.second->SendAudioFrame(frame, frame_size);
	}
}
