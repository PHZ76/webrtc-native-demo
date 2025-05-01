#include "rtc_server.h"
#include "rtc_common.h"

RtcServer::RtcServer()
	: event_loop_(new xop::EventLoop())
{

}

RtcServer::~RtcServer()
{

}

bool RtcServer::Init(RtcConfig config)
{
	local_ip_ = config.local_ip;
	enable_h264_ = config.enable_h264;
	enable_opus_ = config.enable_opus;

	event_loop_->Loop();
	return true;
}

void RtcServer::Destroy()
{
	if (event_loop_) {
		event_loop_->Quit();
	}
}

bool RtcServer::SendVideoFrame(uint8_t* frame, size_t frame_size)
{
	std::lock_guard<std::mutex> locker(connections_mutex_);

	if (rtc_connections_.empty()) {
		return false;
	}

	for (auto conn : rtc_connections_) {
		conn.second->SendVideoFrame(frame, frame_size);
	}

	return true;
}

bool RtcServer::SendAudioFrame(uint8_t* frame, size_t frame_size)
{
	std::lock_guard<std::mutex> locker(connections_mutex_);

	if (rtc_connections_.empty()) {
		return false;
	}

	for (auto conn : rtc_connections_) {
		conn.second->SendAudioFrame(frame, frame_size);
	}

	return true;
}

void RtcServer::OnRequest(std::string stream_name, std::string offer, std::string& answer)
{
	std::lock_guard<std::mutex> locker(connections_mutex_);

	auto rtc_connection = std::make_shared<RtcConnection>(event_loop_);
	rtc_connection->SetStreamName(stream_name);
	rtc_connection->SetLocalAddress(local_ip_, 10000);
	rtc_connection->Init(RTC_ROLE_SERVER);
	rtc_connection->SetRemoteSdp(offer);
	answer = rtc_connection->GetLocalSdp();
	std::string ufrag = rtc_connection->GetLocalUfrag();

	if (answer.size() > 0 && ufrag.size() > 0) {
		rtc_connections_[ufrag] = rtc_connection;
	}
}
