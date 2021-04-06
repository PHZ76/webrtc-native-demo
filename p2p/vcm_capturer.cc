/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vcm_capturer.h"

#include <stdint.h>
#include <memory>

#include "modules/video_capture/video_capture_factory.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_rotation.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

VcmCapturer::VcmCapturer() 
    : vcm_(nullptr) 
{

}

bool VcmCapturer::Init(size_t capture_device_index,
                       size_t target_fps,
                       size_t width,
                       size_t height) 
{
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> device_info(webrtc::VideoCaptureFactory::CreateDeviceInfo());

    char device_name[256];
    char unique_name[256];
    if (device_info->GetDeviceName(static_cast<uint32_t>(capture_device_index),
                                    device_name, sizeof(device_name), unique_name,
                                    sizeof(unique_name)) != 0) {
    Destroy();
    return false;
    }

    vcm_ = webrtc::VideoCaptureFactory::Create(unique_name);
    if (!vcm_) {
        return false;
    }

    vcm_->RegisterCaptureDataCallback(this);

    device_info->GetCapability(vcm_->CurrentDeviceName(), 0, capability_);

    capability_.width = static_cast<int32_t>(width);
    capability_.height = static_cast<int32_t>(height);
    capability_.maxFPS = static_cast<int32_t>(target_fps);
    capability_.videoType = webrtc::VideoType::kI420;

    if (vcm_->StartCapture(capability_) != 0) {
        Destroy();
        return false;
    }

    RTC_CHECK(vcm_->CaptureStarted());
    return true;
}

std::unique_ptr<VcmCapturer> VcmCapturer::Create(size_t capture_device_index,
                                                 size_t target_fps,
                                                 size_t width,
                                                 size_t height)
{
    std::unique_ptr<VcmCapturer> vcm_capturer(new VcmCapturer());
    if (!vcm_capturer->Init(capture_device_index, target_fps, width, height)) {
        RTC_LOG(LS_WARNING) << "Failed to create VcmCapturer(w = " << width
                            << ", h = " << height << ", fps = " << target_fps
                            << ")";
        return nullptr;
    }
    return vcm_capturer;
}

VcmCapturer::~VcmCapturer() {
    Destroy();
}

void VcmCapturer::Destroy() {
    if (!vcm_) {
        return;
    }
    
    vcm_->StopCapture();
    vcm_->DeRegisterCaptureDataCallback();
    vcm_ = nullptr;
}

void VcmCapturer::OnFrame(const webrtc::VideoFrame& frame)
{
    int cropped_width = 0;
    int cropped_height = 0;
    int out_width = 0;
    int out_height = 0;

    if (!video_adapter_.AdaptFrameResolution(
        frame.width(), frame.height(), frame.timestamp_us() * 1000,
        &cropped_width, &cropped_height, &out_width, &out_height)) {
        // Drop frame in order to respect frame rate constraint.
        return;
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

void VcmCapturer::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants)
{
    broadcaster_.AddOrUpdateSink(sink, wants);
    UpdateVideoAdapter();
}

void VcmCapturer::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
    broadcaster_.RemoveSink(sink);
    UpdateVideoAdapter();
}

void VcmCapturer::UpdateVideoAdapter()
{
    video_adapter_.OnSinkWants(broadcaster_.wants());
}
