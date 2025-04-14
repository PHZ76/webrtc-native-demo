#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "http/httplib.h"

struct SignalingConfig
{
	uint16_t port = 18080;
	std::string host = "localhost";

	std::string cert_path;
	std::string key_path;
};

class SignalingHandler
{
public:
	virtual void GetLocalDescription(std::string uid, std::string& local_sdp) {}
	virtual void OnRemoteDescription(std::string uid, std::string remote_sdp) {}
	virtual void SendVideoFrame(uint8_t* frame, size_t frame_size) {}
	virtual void SendAudioFrame(uint8_t* frame, size_t frame_size) {}
};

class SignalingServer
{
public:
	SignalingServer();
	virtual ~SignalingServer();

	bool Init(const SignalingConfig& config, std::shared_ptr<SignalingHandler> handler);
	void Destroy();

private:
	SignalingConfig signaling_config_;
	std::shared_ptr<SignalingHandler> signaling_handler_;

	std::shared_ptr<httplib::Server> server_;
	std::shared_ptr<std::thread> poll_thread_;
};
