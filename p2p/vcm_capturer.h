/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#pragma once

#include <memory>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "modules/video_capture/video_capture.h"
#include "media/base/video_adapter.h"
#include "media/base/video_broadcaster.h"
#include "rtc_base/critical_section.h"
#include "pc/video_track_source.h"

class VcmCapturer : public rtc::VideoSourceInterface<webrtc::VideoFrame>
                  , public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
public:
    static std::unique_ptr<VcmCapturer> Create(size_t capture_device_index, size_t target_fps, size_t width, size_t height);
    virtual ~VcmCapturer();

    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,const rtc::VideoSinkWants& wants) override;
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;
    void UpdateVideoAdapter();

private:
    VcmCapturer();
	bool Init(size_t capture_device_index,
			  size_t target_fps,
			  size_t width,
			  size_t height);
    void Destroy();
    void OnFrame(const webrtc::VideoFrame& frame) override;

    rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm_;
    webrtc::VideoCaptureCapability capability_;

    rtc::VideoBroadcaster broadcaster_;
    cricket::VideoAdapter video_adapter_;
};


class TestVcmCapturer : public webrtc::VideoTrackSource
{
public:
	static rtc::scoped_refptr<TestVcmCapturer> Create()
	{
		const size_t width = 640;
		const size_t height = 480;
		const size_t fps = 30;

		auto vcm_capture = VcmCapturer::Create(0, fps, width, height);
		if (vcm_capture) {
			return new rtc::RefCountedObject<TestVcmCapturer>(std::move(vcm_capture));
		}
		return nullptr;
	}

protected:
	explicit TestVcmCapturer(std::unique_ptr<VcmCapturer> capture)
		: VideoTrackSource(false)
		, capture_(std::move(capture))
	{

	}

private:
	rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override
	{
		return capture_.get();
	}

	std::unique_ptr<VcmCapturer> capture_;
};
