#include "audio_resampler.h"

extern "C" {
#include "libavutil/error.h"
}

#define AV_LOG(code, format, ...)  	\
{								\
	char buf[1024] = { 0 };		\
	av_strerror(code, buf, 1023); \
    printf("[%s:%d] " format " - %s. \n", \
		   __FUNCTION__ , __LINE__, ##__VA_ARGS__, buf);     \
}

AudioResampler::AudioResampler()
	: in_format_(AV_SAMPLE_FMT_NONE)
	, out_format_(AV_SAMPLE_FMT_NONE)
{

}

AudioResampler::~AudioResampler()
{
	Destroy();
}

bool AudioResampler::Init(int in_samplerate, int in_channels, AVSampleFormat in_format,
	int out_samplerate, int out_channels, AVSampleFormat out_format)
{

	if (swr_context_ != nullptr) {
		return false;
	} 

	in_channels_layout_ = (int)av_get_default_channel_layout(in_channels);
	out_channels_layout_ = (int)av_get_default_channel_layout(out_channels);

	swr_context_ = swr_alloc();

	av_opt_set_int(swr_context_, "in_channel_layout", in_channels_layout_, 0);
	av_opt_set_int(swr_context_, "in_sample_rate", in_samplerate, 0);
	av_opt_set_sample_fmt(swr_context_, "in_sample_fmt", in_format, 0);

	av_opt_set_int(swr_context_, "out_channel_layout", out_channels_layout_, 0);
	av_opt_set_int(swr_context_, "out_sample_rate", out_samplerate, 0);
	av_opt_set_sample_fmt(swr_context_, "out_sample_fmt", out_format, 0);


	int ret = swr_init(swr_context_);
	if (ret < 0) {
		AV_LOG(ret, "swr_init() failed.");
		return false;
	}

	in_samplerate_ = in_samplerate;
	in_channels_ = in_channels;
	in_format_ = in_format;
	in_bytes_per_sample_ = av_get_bytes_per_sample(in_format_);
	out_samplerate_ = out_samplerate;
	out_channels_ = out_channels;
	out_format_ = out_format;
	out_bytes_per_sample_ = av_get_bytes_per_sample(out_format_);

	return true;
}

void AudioResampler::Destroy()
{
	if (swr_context_ != nullptr) {
		if (swr_is_initialized(swr_context_)) {
			swr_close(swr_context_);
		}

		swr_free(&swr_context_);
		swr_context_ = nullptr;		
	}
}

int AudioResampler::Convert(uint8_t* in_buffer, int in_size, uint8_t* out_buffer, int out_buffer_size)
{
	if (swr_context_ == nullptr) {
		return -1;
	}

	AVFramePtr in_frame(av_frame_alloc(), [](AVFrame* ptr) { av_frame_free(&ptr); });
	in_frame->sample_rate = in_samplerate_;
	in_frame->format = in_format_;
	in_frame->channels = in_channels_;
	in_frame->channel_layout = in_channels_layout_;
	in_frame->nb_samples = in_size / in_channels_ / in_bytes_per_sample_;
	in_frame->pts = 0;

	if (av_frame_get_buffer(in_frame.get(), 0) < 0) {
		return -2;
	}

	memcpy(in_frame->data[0], in_buffer, in_size);

	AVFramePtr out_frame = nullptr;
	int frame_sample = this->Convert(in_frame, out_frame);
	if (frame_sample <= 0) {
		return -3;
	}

	int frame_size = frame_sample * out_channels_ * out_bytes_per_sample_;
	if (frame_size > out_buffer_size) {
		return -4;
	}

	memcpy(out_buffer, out_frame->data[0], frame_size);
	return frame_size;
}

int AudioResampler::Convert(AVFramePtr in_frame, AVFramePtr& out_frame)
{
	if (swr_context_ == nullptr) {
		return -1;
	}

	out_frame.reset(av_frame_alloc(), [](AVFrame* ptr) {
		av_frame_free(&ptr);
	});

	out_frame->sample_rate = out_samplerate_;
	out_frame->format = out_format_;
	out_frame->channels = out_channels_;
	out_frame->channel_layout = out_channels_layout_;
	out_frame->nb_samples = (int)av_rescale_rnd(in_frame->nb_samples, out_frame->sample_rate, in_frame->sample_rate, AV_ROUND_UP);
	out_frame->pts = out_frame->pkt_dts = in_frame->pts;

	if (av_frame_get_buffer(out_frame.get(), 0) != 0) {
		return -1;
	}

	int len = swr_convert(swr_context_, (uint8_t**)&out_frame->data, out_frame->nb_samples, 
						 (const uint8_t**)in_frame->data, in_frame->nb_samples);
	if (len < 0) {
		out_frame = nullptr;
		AV_LOG(len, "swr_convert() failed.");
		return - 1;
	}

	return len;
}


