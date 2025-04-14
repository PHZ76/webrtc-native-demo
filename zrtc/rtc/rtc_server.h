#pragma once

#include "rtc_utils.h"
#include "rtc_connection.h"
#include <mutex>

struct RtcConfig
{
	bool enable_h264 = true;
	bool enable_opus = true;

	std::string local_ip;
};

class RtcServer
{
public:
	RtcServer();
	virtual ~RtcServer();

	bool Init(RtcConfig config);
	void Destroy();

	bool SendVideoFrame(uint8_t* frame, size_t frame_size);
	bool SendAudioFrame(uint8_t* frame, size_t frame_size);

	void OnRequest(std::string stream_name, std::string offer, std::string& answer);

private:
	bool enable_h264_ = true;
	bool enable_opus_ = true;
	std::string local_ip_ = "127.0.0.1";
	std::shared_ptr<xop::EventLoop> event_loop_;

	std::mutex connections_mutex_;
	std::unordered_map<std::string, std::shared_ptr<RtcConnection>> rtc_connections_;
};
