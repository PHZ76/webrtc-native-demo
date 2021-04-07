#include "rtc_log.h"
#include "p2p_server.h"
#include "p2p_client.h"
#include "desktop_capturer.h"
#include "vcm_capturer.h"
#include "video_renderer.h"

static uint16_t port = 18000;
static int window_width  = 1280;
static int window_height = 720;

void TestClient(std::string title_name)
{
	P2PClient p2p_client;
	std::shared_ptr<webrtc::test::VideoRenderer> renderer(webrtc::test::VideoRenderer::Create(title_name.c_str(), window_width, window_height));

	p2p_client.SetVideoRenderer(renderer);
	if (!p2p_client.Connect(port, "localhost", 5000)) {
		LOG_INFO("p2p client connect failed. \n");
	}

	while (p2p_client.IsConnected()) {
		MSG msg;
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	p2p_client.Disconnect();
}

int main(int argc, char** argv)
{
	P2PServer p2p_server;
#if 1
	// screen capture
	auto video_source = TestDesktopCapturer::Create();
#else 
	// camera capture
	auto video_source = TestVcmCapturer::Create();
#endif

	if (!video_source) {
		return -1;
	}

	p2p_server.SetVideoSource(video_source);
	if (!p2p_server.Start(port, "localhost")) {
		LOG_INFO("p2p server satrt failed. \n");
		return -1;
	}

	int clients = 1;
	std::vector<std::shared_ptr<std::thread>> client_threads;

	for (int i = 0; i < clients; i++) {
		std::string title_name = "p2p-client " + std::to_string(i);
		std::shared_ptr<std::thread> thread(new std::thread([title_name] {
			TestClient(title_name);
		}));
		client_threads.push_back(thread);
	}

	for (auto& iter : client_threads) {
		iter->join();
	}

	p2p_server.Stop();
	return 0;
}
