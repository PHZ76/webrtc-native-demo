#include "p2p_server.h"
#include "rtc_log.h"
#include "api/create_peerconnection_factory.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"

#include <functional>
#include <string>

P2PServer::P2PServer()
{

}

P2PServer::~P2PServer()
{
    Stop();
}	

void P2PServer::SetVideoSource(rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source)
{
	video_source_ = video_source;
}

bool P2PServer::Start(uint16_t port, std::string ip)
{
	if (connection_) {
		return false;
	}

	std::string url = "http://" + ip + ":" + std::to_string(port);

	mg_mgr_init(&mgr_);
	connection_ = mg_http_listen(&mgr_, url.c_str(), P2PServer::OnEvent, this);
	if (!connection_) {
		return false;
	}

	poll_thread_.reset(new std::thread([this] {
		signaling_thread_ = rtc::Thread::Create();
		signaling_thread_->Start();

		peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
			nullptr /* network_thread */, nullptr /* worker_thread */,
			signaling_thread_.get(), nullptr /* default_adm */,
			webrtc::CreateBuiltinAudioEncoderFactory(),
			webrtc::CreateBuiltinAudioDecoderFactory(),
			webrtc::CreateBuiltinVideoEncoderFactory(),
			webrtc::CreateBuiltinVideoDecoderFactory(),
			nullptr /* audio_mixer */,
			nullptr /* audio_processing */);

		audio_source_= peer_connection_factory_->CreateAudioSource(cricket::AudioOptions());

		while (connection_) {
			mg_mgr_poll(&mgr_, 1000);
		}
	
		mutex_.lock();
		p2p_sessios_.clear();
		mutex_.unlock();

		peer_connection_factory_ = nullptr;
		signaling_thread_->Stop();
	}));

	return true;
}

void P2PServer::Stop()
{
	if (connection_) {
		connection_ = nullptr;
		poll_thread_->join();
		poll_thread_.reset();
		mg_mgr_free(&mgr_);
	}
}

void P2PServer::OnEvent(struct mg_connection* conn, int ev, void* ev_data, void* user_data)
{
	P2PServer* server = reinterpret_cast<P2PServer*>(user_data);
	if (!server) {
		return;
	}

	std::lock_guard<std::mutex> locker(server->mutex_);

	if (ev == MG_EV_WS_MSG) {
		// Got websocket frame.
		struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;
		auto iter = server->p2p_sessios_.find(conn);
		if (iter != server->p2p_sessios_.end()) {
			iter->second->OnRead(std::string(wm->data.ptr, wm->data.len));
		}
		mg_iobuf_delete(&conn->recv, conn->recv.len);
	}
	else if (ev == MG_EV_HTTP_MSG) {
		struct mg_http_message* hm = (struct mg_http_message*)ev_data;
		if (mg_http_match_uri(hm, "/websocket")) {
			// Upgrade to websocket
			mg_ws_upgrade(conn, hm, NULL);

			if (server->p2p_sessios_.find(conn) == server->p2p_sessios_.end()) {
				webrtc::PeerConnectionFactoryInterface* peer_connection_factory = server->peer_connection_factory_.get();
				rtc::scoped_refptr<P2PSession> session(new rtc::RefCountedObject<P2PSession>(peer_connection_factory));
				session->SetWriteCallback([conn](std::string message) {
					mg_ws_send(conn, message.c_str(), message.size(), WEBSOCKET_OP_TEXT);
				});
				//session->AddAudioSource(server->audio_source_);
				session->AddVideoSource(server->video_source_);
				server->p2p_sessios_[conn] = session;
			}
		}
	}
	else if (ev == MG_EV_ACCEPT) {

	}
	else if (ev == MG_EV_CLOSE) {
		server->p2p_sessios_.erase(conn);
	}
}
