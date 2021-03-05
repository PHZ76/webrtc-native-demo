#pragma once

extern "C" {
#include "libavutil/audio_fifo.h"
}
#include <cstdint>
#include <memory>
#include <mutex>

class AudioFifo
{
public:
	AudioFifo(AVSampleFormat sample_fmt, int channels, int max_samples);
	virtual ~AudioFifo();

	int GetSamples();
	int Write(void* data, int samples);
	int Read(void* data, int samples);

private:
	std::mutex mutex_;
	std::shared_ptr<AVAudioFifo> audio_fifo_;
	AVSampleFormat sample_fmt_;
	int channels_;
	int max_samples_;
};

