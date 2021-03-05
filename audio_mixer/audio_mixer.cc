#include "audio_mixer.h"

AudioMixer::AudioSource::AudioSource(int ssrc, int sample_rate, int channels, int max_samples)
	: ssrc_(ssrc)
	, sample_rate_(sample_rate)
	, channels_(channels)
	, max_samples_(max_samples)
{
	audio_buffer_.reset(new AudioBuffer(sample_rate_, channels_, BYTES_PER_SAMPLE, max_samples_));
	samples_per_channel_ = sample_rate_ / ((1000 / webrtc::AudioMixerImpl::kFrameDurationInMs));
}

webrtc::AudioMixer::Source::AudioFrameInfo 
AudioMixer::AudioSource::GetAudioFrameWithInfo(int target_rate_hz, webrtc::AudioFrame* frame)
{
	if (target_rate_hz != sample_rate_) {
		return AudioFrameInfo::kError;
	}

	frame->samples_per_channel_ = samples_per_channel_;
	frame->num_channels_ = channels_;
	frame->sample_rate_hz_ = target_rate_hz;

	int samples_to_read = samples_per_channel_;
	if (audio_buffer_->GetSamples() < samples_to_read) {
		return AudioFrameInfo::kMuted;
	}

	audio_buffer_->Read(reinterpret_cast<uint8_t*>(frame->mutable_data()), samples_to_read);

	// Output volume scaling
	if (output_gain_ < 0.99f || output_gain_ > 1.01f) {
		webrtc::AudioFrameOperations::ScaleWithSat(output_gain_, frame);
	}

	return AudioFrameInfo::kNormal;
}

int AudioMixer::AudioSource::Ssrc() const
{
	return ssrc_;
}

int AudioMixer::AudioSource::PreferredSampleRate() const
{
	return sample_rate_;
}

int AudioMixer::AudioSource::AddFrame(void* in_buffer, int in_samples)
{	
	return audio_buffer_->Write(reinterpret_cast<uint8_t*>(in_buffer), in_samples);
}

int AudioMixer::AudioSource::GetSamples()
{
	return audio_buffer_->GetSamples();
}

void AudioMixer::AudioSource::SetVolume(float volume)
{
	output_gain_ = volume;
}

AudioMixer::AudioMixer(int sample_rate, int channels)
	: sample_rate_(sample_rate)
	, channels_(channels)
{
	audio_mixer_impl_ = webrtc::AudioMixerImpl::Create(
		std::unique_ptr<webrtc::OutputRateCalculator>(
			new webrtc::DefaultOutputRateCalculator()), true);

	samples_per_channel_ = sample_rate / (1000 / webrtc::AudioMixerImpl::kFrameDurationInMs);
	max_samples_ = sample_rate * 10;
}

AudioMixer::~AudioMixer()
{
	
}

bool AudioMixer::AddSource(int ssrc)
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (sources_.find(ssrc) != sources_.end()) {
		return false;
	}

	auto source = std::make_shared<AudioSource>(ssrc, sample_rate_, channels_, max_samples_);
	sources_[ssrc] = source;
	audio_mixer_impl_->AddSource(source.get());
	return true;
}

void AudioMixer::RemoveSource(int ssrc)
{
	std::lock_guard<std::mutex> locker(mutex_);

	auto iter = sources_.find(ssrc);
	if (iter == sources_.end()) {
		return ;
	}

	audio_mixer_impl_->RemoveSource(iter->second.get());
	sources_.erase(ssrc);
}

void AudioMixer::SetVolume(int ssrc, float volume)
{
	std::shared_ptr<AudioSource> source = nullptr;

	mutex_.lock();
	auto iter = sources_.find(ssrc);
	if (iter != sources_.end()) {
		source = std::dynamic_pointer_cast<AudioSource>(iter->second);
	}
	mutex_.unlock();

	if (source) {
		source->SetVolume(volume);
	}
}

int AudioMixer::AddFrame(int ssrc, void* in_buffer, size_t in_size)
{
	std::shared_ptr<AudioSource> source = nullptr;

	mutex_.lock();
	auto iter = sources_.find(ssrc);
	if (iter != sources_.end()) {
		source = std::dynamic_pointer_cast<AudioSource>(iter->second);
	}
	mutex_.unlock();

	if (source) {
		if (source->GetSamples() >= max_samples_) {
			return -1;
		}
		return source->AddFrame(in_buffer, static_cast<int>(in_size) / channels_ / BYTES_PER_SAMPLE);
	}

	return 0;
}

int AudioMixer::MixFrame(void* out_buffer, size_t out_buffer_size)
{
	mutex_.lock();
	if (sources_.empty()) {
		mutex_.unlock();
		return -1;
	}
	mutex_.unlock();

	audio_mixer_impl_->Mix(channels_, &audio_frame_);

	int audio_frame_size = static_cast<int>(audio_frame_.samples_per_channel_) * channels_ * BYTES_PER_SAMPLE;
	if (audio_frame_size > out_buffer_size) {
		return -2;
	}

	memcpy(out_buffer, audio_frame_.data(), audio_frame_size);
	return audio_frame_size;
}

int AudioMixer::GetSampleRate() const
{
	return sample_rate_;	
}

int AudioMixer::GetChannels() const
{
	return channels_;
}

int AudioMixer::GetBytesPerSample() const
{
	return BYTES_PER_SAMPLE;
}
