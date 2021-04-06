#include "p2p_client.h"
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

P2PClient::P2PClient()
{

}

P2PClient::~P2PClient()
{

}

void P2PClient::SetVideoRenderer(std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> video_renderer)
{
	video_renderer_ = video_renderer;
}

bool P2PClient::Connect(uint16_t port, std::string ip, int timeout_msec)
{
	if (connection_) {
		return false;
	}

	std::string url = "ws://" + ip + ":" + std::to_string(port) + "/websocket";
	mg_mgr_init(&mgr_);
	connection_ = mg_ws_connect(&mgr_, url.c_str(), P2PClient::OnEvent, this, NULL);   
	is_connected_ = false;

	if (!connection_) {
		return false;
	}

	if (timeout_msec > 0) {
		mg_mgr_poll(&mgr_, timeout_msec);
		if (!is_connected_) {
			connection_ = nullptr;
			mg_mgr_free(&mgr_);
			return false;
		}
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

		while (connection_) {
			mg_mgr_poll(&mgr_, 100);
		}

		p2p_session_ = nullptr;
		peer_connection_factory_ = nullptr;
		signaling_thread_->Stop();
	}));

	return true;
}

void P2PClient::Disconnect()
{
	if (connection_) {
		connection_ = nullptr;
		poll_thread_->join();
		poll_thread_.reset();
		mg_mgr_free(&mgr_);
		is_connected_ = false;
	}
}

bool P2PClient::IsConnected() const
{
	return is_connected_;
}

void P2PClient::Poll()
{
	mg_mgr_poll(&mgr_, 1000);
}

void P2PClient::OnEvent(struct mg_connection* conn, int ev, void* ev_data, void* user_data)
{
	P2PClient* client = reinterpret_cast<P2PClient*>(user_data);
	if (!client) {
		return;
	}

	if (ev == MG_EV_WS_MSG) {
		struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;
		if (client->p2p_session_) {
			client->p2p_session_->OnRead(std::string(wm->data.ptr, wm->data.len));
		}
	}
	else if (ev == MG_EV_CONNECT) {
		client->is_connected_ = true;
	}
	else if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE) {
		client->p2p_session_ = nullptr;
	}
	else if (ev == MG_EV_WS_OPEN) {
		webrtc::PeerConnectionFactoryInterface* peer_connection_factory = client->peer_connection_factory_.get();
		client->p2p_session_ = rtc::scoped_refptr<P2PSession>(new rtc::RefCountedObject<P2PSession>(peer_connection_factory));
		client->p2p_session_->SetWriteCallback([conn](std::string message) {
			mg_ws_send(conn, message.c_str(), message.size(), WEBSOCKET_OP_TEXT);
		});
		client->p2p_session_->AddVideoRenderer(client->video_renderer_);
		client->p2p_session_->SendOffer();
	}
}
