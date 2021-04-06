#include "desktop_capturer.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_rotation.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/sleep.h"
#include "third_party/libyuv/include/libyuv.h"

#ifdef WIN32
#include <Mmsystem.h>
#endif

static webrtc::DesktopCaptureOptions GetCaptureOption()
{
    auto capture_options = webrtc::DesktopCaptureOptions::CreateDefault();
#if defined(_WIN32)
    capture_options.set_allow_directx_capturer(true);
    capture_options.set_allow_use_magnification_api(false);
#elif defined(__APPLE__)
    capture_options.set_allow_iosurface(true);
#endif
    return capture_options;
}

bool DesktopCapturer::GetScreenSourceList(webrtc::DesktopCapturer::SourceList& source_list)
{
   auto desktop_capturer = webrtc::DesktopCapturer::CreateScreenCapturer(GetCaptureOption());
   return desktop_capturer->GetSourceList(&source_list);
}

bool DesktopCapturer::GetWindowSourceList(webrtc::DesktopCapturer::SourceList& source_list)
{
    auto desktop_capturer = webrtc::DesktopCapturer::CreateWindowCapturer(GetCaptureOption());
    return desktop_capturer->GetSourceList(&source_list);
}

std::unique_ptr<DesktopCapturer> DesktopCapturer::Create(SourceId source_id, size_t target_fps, size_t out_width, size_t out_height)
{
    std::unique_ptr<DesktopCapturer> screen_capture(new DesktopCapturer());
    if (!screen_capture->Init(source_id, target_fps, out_width, out_height)) {
        RTC_LOG(LS_WARNING) << "Failed to create VcmCapturer(w = " << out_width
            << ", h = " << out_height << ", fps = " << target_fps
            << ")";
        return nullptr;
    }
    return screen_capture;
}

DesktopCapturer::DesktopCapturer()
{
    capture_options_ = webrtc::DesktopCaptureOptions::CreateDefault();
#if defined(_WIN32)
    capture_options_.set_allow_directx_capturer(true);
    capture_options_.set_allow_use_magnification_api(false);
#elif defined(__APPLE__)
    capture_options_.set_allow_iosurface(true);
#endif
}

DesktopCapturer::~DesktopCapturer()
{
    Destroy();
}

void DesktopCapturer::SetFrameCallback(const FrameCallback& frame_callback)
{
    frame_callback_ = frame_callback;
}

bool DesktopCapturer::Init(webrtc::DesktopCapturer::SourceId source_id, size_t target_fps, size_t out_width, size_t out_height)
{
    if (is_capturing_) {
        return false;
    }

    if (target_fps == 0) {
        return false;
    }

    target_fps_ = target_fps;
    out_width_ = out_width;
    out_height_ = out_height;

    if (!CreateCapture(source_id)) {
        RTC_LOG(LS_WARNING) << "Failed to found capturer.";
        return false;
    }

    if (is_capture_cursor_) {
        desktop_capturer_.reset(new webrtc::DesktopAndCursorComposer(std::move(desktop_capturer_), GetCaptureOption()));
    }

    if (!desktop_capturer_->SelectSource(source_id_)) {
        RTC_LOG(LS_WARNING) << "Failed to select source.";
        return false;
    }

    is_capturing_ = true;
    capture_thread_.reset(new std::thread([this] {
        CaptureThread();
    }));

    return true;
}

void DesktopCapturer::Destroy()
{
    if (is_capturing_) {
        is_capturing_ = false;
        capture_thread_->join();
        capture_thread_.reset();
    }

    if (desktop_capturer_) {
        desktop_capturer_.reset();
    }
}

bool DesktopCapturer::CreateCapture(webrtc::DesktopCapturer::SourceId source_id)
{
    bool is_found = false;
    webrtc::DesktopCapturer::SourceList source_list;
    webrtc::DesktopCaptureOptions options = GetCaptureOption();

    source_list.clear();
    desktop_capturer_ = webrtc::DesktopCapturer::CreateScreenCapturer(options);
    if (desktop_capturer_->GetSourceList(&source_list)) {
        for (auto iter : source_list) {
            if (source_id == iter.id) {
                source_id_ = source_id;
                title_ = iter.title;
                is_found = true;
                break;
            }
        }
    }

    if (is_found) {
        return true;
    }

    source_list.clear();
    desktop_capturer_ = webrtc::DesktopCapturer::CreateWindowCapturer(options);
    if (desktop_capturer_->GetSourceList(&source_list)) {
        for (auto iter : source_list) {
            if (source_id == iter.id) {
                source_id_ = source_id;
                title_ = iter.title;
                is_found = true;
                break;
            }
        }
    }

    if (!is_found) {
        desktop_capturer_.reset();
    }
 
    return is_found;
}

void DesktopCapturer::CaptureThread()
{
    int64_t last_ts = 0;
    int64_t frame_duration_ms = 1000 / target_fps_;

    desktop_capturer_->Start(this);

    while (is_capturing_) {
        int64_t now_ts = rtc::TimeMillis();
        if (last_ts + frame_duration_ms <= now_ts) {
            last_ts = now_ts;        
            desktop_capturer_->CaptureFrame(); 
        }

        int64_t delta_time_ms = frame_duration_ms - (rtc::TimeMillis() - now_ts);
        if (delta_time_ms <= 0) {
            delta_time_ms = 1;
        }

        if (delta_time_ms > 0) {
#ifdef WIN32
            timeBeginPeriod(1);
            Sleep(static_cast<int>(delta_time_ms));
            timeEndPeriod(1);
#else 
            webrtc::SleepMs(delta_time_ms);       
#endif
        }
    }
}

void DesktopCapturer::OnCaptureResult(webrtc::DesktopCapturer::Result result, std::unique_ptr<webrtc::DesktopFrame> frame)
{
    if (result != webrtc::DesktopCapturer::Result::SUCCESS) {
        RTC_LOG(LS_ERROR) << "Capture frame faiiled, result: " << result;
        return;
    }

    int width = frame->size().width();
    int height = frame->size().height();
    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer = webrtc::I420Buffer::Create(width, height);

    libyuv::ConvertToI420(frame->data(), 0, i420_buffer->MutableDataY(),
        i420_buffer->StrideY(), i420_buffer->MutableDataU(),
        i420_buffer->StrideU(), i420_buffer->MutableDataV(),
        i420_buffer->StrideV(), 0, 0, width, height,
        i420_buffer->width(), i420_buffer->height(),
        libyuv::kRotate0, libyuv::FOURCC_ARGB);

    auto video_frame = webrtc::VideoFrame(i420_buffer, webrtc::kVideoRotation_0, rtc::TimeMicros());
    if (frame_callback_) {
        frame_callback_(video_frame);
    }

    OnFrame(video_frame);
}

void DesktopCapturer::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants)
{
    broadcaster_.AddOrUpdateSink(sink, wants);
    UpdateVideoAdapter();
}

void DesktopCapturer::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
    broadcaster_.RemoveSink(sink);
    UpdateVideoAdapter();
}

void DesktopCapturer::UpdateVideoAdapter() {
    video_adapter_.OnSinkWants(broadcaster_.wants());
}

void DesktopCapturer::OnFrame(const webrtc::VideoFrame& frame)
{
    int cropped_width = 0;
    int cropped_height = 0;
    int out_width = 0;
    int out_height = 0;

    // process frame 
    // ...

    if (!video_adapter_.AdaptFrameResolution(
        frame.width(), frame.height(), frame.timestamp_us() * 1000,
        &cropped_width, &cropped_height, &out_width, &out_height)) {
        // Drop frame in order to respect frame rate constraint.
        return;
    }

    if (out_height_ > 0 && out_width_ > 0) {
        out_height = static_cast<int>(out_height_);
        out_width = static_cast<int>(out_width_);
    }

    if (out_height != frame.height() || out_width != frame.width()) {
        // Video adapter has requested a down-scale. Allocate a new buffer and
        // return scaled version.
        // For simplicity, only scale here without cropping.
        rtc::scoped_refptr<webrtc::I420Buffer> scaled_buffer = webrtc::I420Buffer::Create(out_width, out_height);

        scaled_buffer->ScaleFrom(*frame.video_frame_buffer()->ToI420());

        webrtc::VideoFrame::Builder new_frame_builder =
            webrtc::VideoFrame::Builder()
                .set_video_frame_buffer(scaled_buffer)
                .set_rotation(webrtc::kVideoRotation_0)
                .set_timestamp_us(frame.timestamp_us())
                .set_id(frame.id());

        if (frame.has_update_rect()) {
            webrtc::VideoFrame::UpdateRect new_rect = frame.update_rect().ScaleWithFrame(
                frame.width(), frame.height(), 0, 0, frame.width(), frame.height(),
                out_width, out_height);
            new_frame_builder.set_update_rect(new_rect);
        }
        broadcaster_.OnFrame(new_frame_builder.build());
    }
    else {
        // No adaptations needed, just return the frame as is.
        broadcaster_.OnFrame(frame);
    }
}
