#include "desktop_capturer.h"
#include "test/video_renderer.h"

int main(int argc, char** argv)
{
	webrtc::DesktopCapturer::SourceList source_list;
	DesktopCapturer::GetScreenSourceList(source_list);

	if (source_list.empty()) {
		return -1;
	}

	int total_frames = 0;
	int scaled_width = 1280;
	int scaled_height = 720;

	auto desktop_capturer = DesktopCapturer::Create(source_list[0].id, 60, scaled_width, scaled_height);
	desktop_capturer->SetFrameCallback([&] (const webrtc::VideoFrame& frame){
		total_frames++;
	});

	std::unique_ptr<webrtc::test::VideoRenderer> renderer(webrtc::test::VideoRenderer::Create("", 1280, 720));
	desktop_capturer->AddOrUpdateSink(renderer.get(), rtc::VideoSinkWants());

	MSG msg;	

	while (1) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				break;
			}
				
			TranslateMessage(&msg);	
			DispatchMessage(&msg);	
		}

		Sleep(100);
	}
	
	desktop_capturer->RemoveSink(renderer.get());
	return 0;
}
