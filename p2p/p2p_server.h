#pragma once

#include "p2p_session.h"
#include <cstdint>
#include <map>
#include <mutex>
#include <memory>
#include <thread>
extern "C" {
#include "mongoose.h"
}

class P2PServer
{
public:
	P2PServer& operator=(const P2PServer&) = delete;
	P2PServer(const P2PServer&) = delete;
	P2PServer();
	virtual ~P2PServer();

	void SetVideoSource(rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source);

	bool Start(uint16_t port, std::string ip = "localhost");
	void Stop();

private:
	static void OnEvent(struct mg_connection* conn, int ev, void* ev_data, void* user_data);

	struct mg_mgr mgr_;
	struct mg_connection* connection_ = nullptr;
	std::unique_ptr<std::thread> poll_thread_;

	std::mutex mutex_;
	std::map<mg_connection*, rtc::scoped_refptr<P2PSession>> p2p_sessios_;

	rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>   video_source_;
	rtc::scoped_refptr<webrtc::AudioSourceInterface>        audio_source_;

	std::unique_ptr<rtc::Thread> signaling_thread_;
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
};
