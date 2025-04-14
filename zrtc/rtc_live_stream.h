#pragma once

#include "capture/d3d11_screen_capture.h"
#include "capture/audio_capture.h"
#include "avcodec/h264_encoder.h"
#include "avcodec/opus_encoder.h"
#include "avcodec/audio_resampler.h"

class RtcLiveStream
{
public:
	using VideoCallback = std::function<void(uint8_t* frame, size_t frame_size, uint8_t frame_type)>;
	using AudioCallback = std::function<void(uint8_t* frame, size_t frame_size)>;

	RtcLiveStream();
	virtual ~RtcLiveStream();

	bool Init();
	void Destroy();

	void SetVideoCallback(const VideoCallback& callback);
	void SetAudioCallback(const AudioCallback& callback);

private:
	bool InitVideo();
	bool InitAudio();
	void CaptureVideo();
	void CaptureAudio();

	VideoCallback video_callback_;
	AudioCallback audio_callback_;
	std::shared_ptr<std::thread> video_thread_;
	std::shared_ptr<std::thread> audio_thread_;
	bool start_video_ = false;
	bool start_audio_ = false;

	AVConfig video_config_ = {};
	std::shared_ptr<ffmpeg::H264Encoder> h264_encoder_;
	std::shared_ptr<DX::ScreenCapture> screen_capture_;

	AVConfig audio_config_;
	std::shared_ptr<ffmpeg::Resampler> resampler_;
	std::shared_ptr<OpusAudioEncoder> opus_encoder_;
	std::shared_ptr<AudioCapture> audio_capture_;
};