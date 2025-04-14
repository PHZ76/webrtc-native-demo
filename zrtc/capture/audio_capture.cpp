#include "audio_capture.h"
#include "net/log.h"
#include "net/Timestamp.h"

AudioCapture::AudioCapture()
	: audio_buffer_(new AudioBuffer(48000 * 4))
{
   
}

AudioCapture::~AudioCapture()
{
	Destroy();
}

bool AudioCapture::Init()
{
	if (is_initialized_) {
		return true;
	}

	if (!capture_.Init()) {
		return false;
	}

	WAVEFORMATEX *audioFmt = capture_.GetAudioFormat();
	channels_ = audioFmt->nChannels;
	samplerate_ = audioFmt->nSamplesPerSec;
	bits_per_sample_ = audioFmt->wBitsPerSample;

	if (!player_.Init()) {
		capture_.Destroy();
		return false;
	}

	if (!StartCapture()) {
		return false;
	}
	
	is_initialized_ = true;
	return true;
}

void AudioCapture::Destroy()
{
	if (is_initialized_) {
		StopCapture();
		player_.Destroy();
		capture_.Destroy();
		is_initialized_ = false;
	}
}

bool AudioCapture::StartCapture()
{
	capture_.SetFrameCallback([this](const WAVEFORMATEX *mixFormat, uint8_t *data, uint32_t samples) {
#if 0
		static xop::Timestamp timestamp;
		static int samples_per_second = 0;
		samples_per_second += samples;
		if (timestamp.Elapsed() >= 990) {
			printf("samples per second: %d\n", samples_per_second);
			samples_per_second = 0;
			timestamp.Reset();
		}
#endif
		audio_buffer_->Write((char*)data, mixFormat->nBlockAlign * samples);
	});

	audio_buffer_->Clear();
	player_.SetFrameCallback([this](const WAVEFORMATEX* mixFormat, uint8_t* data, uint32_t samples) {
		memset(data, 0, mixFormat->nBlockAlign * samples);
	});
	player_.StartPlay();
	capture_.StartCapture();
	is_started_ = true;
	return true;
}

void AudioCapture::StopCapture()
{
	capture_.StopCapture();
	player_.StopPlay();
	is_started_ = false;
}

int AudioCapture::Read(uint8_t *data, uint32_t samples)
{
	if ((int)samples > this->GetSamples()) {
		return 0;
	}
	if (bits_per_sample_ <= 0 || channels_ <= 0) {
		return 0;
	}
	audio_buffer_->Read((char*)data, samples * bits_per_sample_ / 8 * channels_);
	return samples;
}

int AudioCapture::GetSamples()
{
	if (bits_per_sample_ <= 0 || channels_ <= 0) {
		return 0;
	}
	return audio_buffer_->Size() * 8 / bits_per_sample_ / channels_;
}
