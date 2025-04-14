#include "signaling_server.h"
#include "rtc_signaling_handler.h"
#include "rtc_live_stream.h"

int main(int argc, char** argv)
{
	std::string host = "127.0.0.1"; // signaling server: http://127.0.0.1:19080/rtc
#if 0
	std::vector<std::string> address = xop::NetInterface::GetLocalIPAddress();
	if (!address.empty()) {
		host = address[0];
	}
#endif
	SignalingConfig signaling_config;
	signaling_config.port = 19080;
	signaling_config.host = host;
	//signaling_config.cert_path = "localhost.pem";
	//signaling_config.key_path = "localhost-key.pem";

	std::unique_ptr<SignalingServer> signaling_server = std::make_unique<SignalingServer>();
	std::shared_ptr<SignalingHandler> signaling_handler = std::make_shared<RtcSignalingHandler>(signaling_config);
	if (!signaling_server->Init(signaling_config, signaling_handler)) {
		return -1;
	}
	RTC_LOG_INFO("start rtc server succeed, addr:{}:{}", signaling_config.host, signaling_config.port);

	std::unique_ptr<RtcLiveStream> rtc_live_stream = std::make_unique<RtcLiveStream>();
	rtc_live_stream->SetVideoCallback([signaling_handler] (uint8_t* frame, size_t frame_size, uint8_t frame_type) {
		signaling_handler->SendVideoFrame(frame, frame_size);
	});
	rtc_live_stream->SetAudioCallback([signaling_handler](uint8_t* frame, size_t frame_size) {
		signaling_handler->SendAudioFrame(frame, frame_size);
	});

	if (!rtc_live_stream->Init()) {
		return -2;
	}
	RTC_LOG_INFO("start rtc live succeed.");

	while (1) {
		xop::Timer::Sleep(10);
	}

	return 0;
}