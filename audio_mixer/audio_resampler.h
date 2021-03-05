#pragma once

extern "C" {
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
#include "libavcodec/avcodec.h"
}

#include <cstdint>
#include <memory>

class AudioResampler
{
public:
	using AVFramePtr = std::shared_ptr<AVFrame>;

	AudioResampler& operator=(const AudioResampler&) = delete;
	AudioResampler(const AudioResampler&) = delete;
	AudioResampler();
	virtual ~AudioResampler();

	bool Init(int in_samplerate, int in_channels, AVSampleFormat in_format, 
		int out_samplerate, int out_channels, AVSampleFormat out_format);

	void Destroy();

	int Convert(uint8_t* in_buffer, int in_size, uint8_t* out_buffer, int out_buffer_size);

private:
	int Convert(AVFramePtr in_frame, AVFramePtr& out_frame);

	SwrContext* swr_context_ = nullptr;
	uint8_t** dst_buf_ = nullptr;

	int in_samplerate_ = 0;
	int in_channels_ = 0;
	int in_channels_layout_ = 0;
	int in_bytes_per_sample_ = 0;
	AVSampleFormat in_format_;

	int out_samplerate_ = 0;
	int out_channels_ = 0;
	int out_channels_layout_ = 0;
	int out_bytes_per_sample_ = 0;
	AVSampleFormat out_format_;
};

