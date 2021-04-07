#pragma once

#include <memory>
#include <thread>
#include "p2p_session.h"
extern "C" {
#include "mongoose.h"
}

class P2PClient
{
public:
	P2PClient& operator=(const P2PClient&) = delete;
	P2PClient(const P2PClient&) = delete;
	P2PClient();
	virtual ~P2PClient();

	void SetVideoRenderer(std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> video_renderer);
	bool Connect(uint16_t port, std::string ip = "localhost", int timeout_msec = -1);
	void Disconnect();
	bool IsConnected() const;
	void Poll();

private:
	static void OnEvent(struct mg_connection* conn, int ev, void* ev_data, void* user_data);

	struct mg_mgr mgr_;  
	struct mg_connection* connection_ = nullptr;

	bool is_connected_ = false;
	std::unique_ptr<std::thread> poll_thread_;
	rtc::scoped_refptr<P2PSession> p2p_session_;

	std::unique_ptr<rtc::Thread> signaling_thread_;
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;

	std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> video_renderer_;
};
