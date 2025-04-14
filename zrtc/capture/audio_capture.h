#ifndef CORE_AUDIO_CAPTURE_H
#define CORE_AUDIO_CAPTURE_H

#include "wasapi_capture.h"
#include "wasapi_player.h"
#include "audio_buffer.h"

class AudioCapture
{
public:
	AudioCapture();
	virtual ~AudioCapture();

	bool Init();
	void Destroy();
	
	int Read(uint8_t*data,uint32_t samples);
	int GetSamples();

	uint32_t GetSamplerate() const
	{ return samplerate_; }

	uint32_t GetChannels()const
	{ return channels_; }

	uint32_t GetBitsPerSample()const
	{ return bits_per_sample_; }

	bool CaptureStarted() const
	{ return is_started_; }

private:
	bool StartCapture();
	void StopCapture();
	
	bool is_initialized_ = false;
	bool is_started_ = false;

	uint32_t channels_ = 2;
	uint32_t samplerate_ = 48000;
	uint32_t bits_per_sample_ = 16;

	WASAPIPlayer player_;
	WASAPICapture capture_;
	std::unique_ptr<AudioBuffer> audio_buffer_;
};

#endif