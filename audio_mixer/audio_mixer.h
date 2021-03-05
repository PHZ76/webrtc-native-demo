#pragma once

#include "api/audio/audio_mixer.h"
#include "common_audio/wav_file.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_mixer/default_output_rate_calculator.h"
#include "audio/utility/audio_frame_operations.h"
#include "audio_buffer.h"
#include <map>

class AudioMixer
{
public:
	class AudioSource : public webrtc::AudioMixer::Source
	{
	public:
		AudioSource(int ssrc, int sample_rate, int channels, int max_samples);

		AudioFrameInfo GetAudioFrameWithInfo(int target_rate_hz, webrtc::AudioFrame* frame) override;

		int Ssrc() const override;

		int PreferredSampleRate() const override;

		int  AddFrame(void* in_buffer, int in_samples);
		int  GetSamples();
		void SetVolume(float volume);

	private:
		int ssrc_;
		int sample_rate_;
		int channels_;
		int max_samples_;
		int samples_per_channel_;
		float output_gain_ = 1.0;
		std::unique_ptr<AudioBuffer> audio_buffer_;
	};

	/*
	* samplerate: 8kHz, 16kHz, 32kHz, 48kHz
	* channels: 2
	* format: sint16
	*/	
	AudioMixer(int sample_rate, int channels);
	AudioMixer& operator=(const AudioMixer&) = delete;
	AudioMixer(const AudioMixer&) = delete;
	virtual ~AudioMixer();

	bool AddSource(int ssrc);
	void RemoveSource(int ssrc);
	void SetVolume(int ssrc, float volume);

	int AddFrame(int ssrc, void* in_buffer, size_t in_size);
	int MixFrame(void* out_buffer, size_t out_buffer_size);

	int GetSampleRate() const;
	int GetChannels() const;
	int GetBytesPerSample() const;

private:
	int sample_rate_;
	int channels_;
	int samples_per_channel_;
	int max_samples_;
	webrtc::AudioFrame audio_frame_;
	rtc::scoped_refptr<webrtc::AudioMixerImpl> audio_mixer_impl_;
	std::mutex mutex_;
	std::map<int, std::shared_ptr<webrtc::AudioMixer::Source>> sources_;

	static const int BYTES_PER_SAMPLE = 2;
};

