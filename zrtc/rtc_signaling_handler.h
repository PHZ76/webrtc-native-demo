#pragma once


#include "rtc/rtc_connection.h"
#include "signaling_server.h"

class RtcSignalingHandler : public SignalingHandler
{
public:
	RtcSignalingHandler(const SignalingConfig& signaling_config);
	~RtcSignalingHandler();

	virtual void GetLocalDescription(std::string uid, std::string& local_sdp);
	virtual void OnRemoteDescription(std::string uid, std::string remote_sdp);
	virtual void SendVideoFrame(uint8_t* frame, size_t frame_size);
	virtual void SendAudioFrame(uint8_t* frame, size_t frame_size);

private:
	SignalingConfig signaling_config_;
	std::shared_ptr<xop::EventLoop> event_loop_;
	std::mutex conns_mutex_;
	std::unordered_map<std::string, std::shared_ptr<RtcConnection>> rtc_conns_;
};
