#pragma once

#include "media/base/video_common.h"
#include "media/base/video_broadcaster.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_and_cursor_composer.h"
#include "media/base/video_adapter.h"
#include "media/base/video_broadcaster.h"
#include <thread>
#include <functional>

class DesktopCapturer : public rtc::VideoSourceInterface<webrtc::VideoFrame>
					  , public webrtc::DesktopCapturer::Callback
{
public:
	using FrameCallback = std::function<void(const webrtc::VideoFrame& frame)>;

	static bool GetScreenSourceList(webrtc::DesktopCapturer::SourceList& screen_source_list);
	static bool GetWindowSourceList(webrtc::DesktopCapturer::SourceList& window_source_list);
	static std::shared_ptr<DesktopCapturer> Create();

	virtual ~DesktopCapturer();

	void SetFrameCallback(const FrameCallback& frame_callback);

	bool Init(webrtc::DesktopCapturer::SourceId source_id, size_t target_fps, size_t out_width = 0, size_t out_height = 0);
	void Destroy();

	void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) override;
	void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;
	void UpdateVideoAdapter();

private:
	DesktopCapturer();
	bool CreateCapture(webrtc::DesktopCapturer::SourceId source_id);
	void CaptureThread();
	void OnCaptureResult(webrtc::DesktopCapturer::Result result, std::unique_ptr<webrtc::DesktopFrame> frame) override;
	void OnFrame(const webrtc::VideoFrame& frame);

	FrameCallback frame_callback_;	
	std::string title_;
	size_t out_width_  = 0;
	size_t out_height_ = 0;
	size_t target_fps_ = 25;
	webrtc::DesktopCaptureOptions capture_options_;
	webrtc::DesktopCapturer::SourceId source_id_;
	std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer_;

	bool is_capturing_ = false;
	bool is_capture_cursor_ = true;
	std::unique_ptr<std::thread> capture_thread_;

	rtc::VideoBroadcaster broadcaster_;
	cricket::VideoAdapter video_adapter_;
};

