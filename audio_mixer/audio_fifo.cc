#include "audio_fifo.h"

AudioFifo::AudioFifo(AVSampleFormat sample_fmt, int channels, int max_samples)
	: sample_fmt_(sample_fmt)
	, channels_(channels)
	, max_samples_(max_samples)
{
	audio_fifo_.reset(av_audio_fifo_alloc(sample_fmt_, channels_, max_samples_),
					  [](AVAudioFifo* af){ av_audio_fifo_free(af); });
}

AudioFifo::~AudioFifo()
{

}

int AudioFifo::GetSamples()
{
	std::lock_guard<std::mutex> locker(mutex_);
	return av_audio_fifo_size(audio_fifo_.get());
}

int AudioFifo::Write(void* data, int samples)
{
	std::lock_guard<std::mutex> locker(mutex_);
	return av_audio_fifo_write(audio_fifo_.get(), &data, samples );
}

int AudioFifo::Read(void* data, int samples)
{
	std::lock_guard<std::mutex> locker(mutex_);
	return av_audio_fifo_read(audio_fifo_.get(), &data, samples );
}