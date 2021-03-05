#include "wav_file_source.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "common_audio/wav_file.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_mixer/default_output_rate_calculator.h"
#include "rtc_base/strings/string_builder.h"

namespace rtc {

WavFileSource::WavFileSource(std::string filename, int ssrc)
	: wav_reader_(new webrtc::WavReader(filename))
	, sample_rate_hz_(wav_reader_->sample_rate())
	, samples_per_channel_(sample_rate_hz_ / 100)
	, number_of_channels_(static_cast<int>(wav_reader_->num_channels())) 
    , ssrc_(ssrc)
{

}

webrtc::AudioMixer::Source::AudioFrameInfo WavFileSource::GetAudioFrameWithInfo(int target_rate_hz, webrtc::AudioFrame* frame)
{
    frame->samples_per_channel_ = samples_per_channel_;
    frame->num_channels_ = number_of_channels_;
    frame->sample_rate_hz_ = target_rate_hz;

    RTC_CHECK_EQ(target_rate_hz, sample_rate_hz_);

    size_t num_to_read = number_of_channels_ * samples_per_channel_;
    size_t num_read = wav_reader_->ReadSamples(num_to_read, frame->mutable_data());

    file_has_ended_ = num_to_read != num_read;
    if (file_has_ended_) {
        frame->Mute();
    }

    return file_has_ended_ ? AudioFrameInfo::kMuted : AudioFrameInfo::kNormal;
}

int WavFileSource::Ssrc() const
{ 
    return ssrc_; 
}

int WavFileSource::PreferredSampleRate() const 
{ 
    return sample_rate_hz_; 
}

bool WavFileSource::FileHasEnded() const 
{ 
    return file_has_ended_; 
}

}

/*

int main(int argc, char** argv)
{
    rtc::scoped_refptr<webrtc::AudioMixerImpl> mixer(
        webrtc::AudioMixerImpl::Create(
            std::unique_ptr<webrtc::OutputRateCalculator>(
            new webrtc::DefaultOutputRateCalculator()),true));

    std::vector<std::shared_ptr<rtc::WavFileSource>> sources;

    auto source1 = std::make_shared<rtc::WavFileSource>("test1.wav", 1);
    auto source2 = std::make_shared<rtc::WavFileSource>("test2.wav", 2);

    mixer->AddSource(source1.get());
    mixer->AddSource(source2.get());

    sources.push_back(source1);
    sources.push_back(source2);

    const int sample_rate = 48000;
    const int num_channels = 2; // stereo

    webrtc::WavWriter wav_writer("mixx.wav", sample_rate, num_channels);
    webrtc::AudioFrame frame;
    bool all_streams_finished = false;

    frame.UpdateFrame(0,
        nullptr, static_cast<size_t>(sample_rate / (1000 / webrtc::AudioMixerImpl::kFrameDurationInMs)),
        sample_rate,
        webrtc::AudioFrame::SpeechType::kUndefined,
        webrtc::AudioFrame::VADActivity::kVadUnknown,
        static_cast<size_t>(num_channels));

    while (!all_streams_finished) {
        mixer->Mix(num_channels, &frame);
        RTC_CHECK_EQ(sample_rate / 100, frame.samples_per_channel_);
        RTC_CHECK_EQ(sample_rate, frame.sample_rate_hz_);
        RTC_CHECK_EQ(num_channels, frame.num_channels_);

        wav_writer.WriteSamples(frame.data(), num_channels * frame.samples_per_channel_);

        all_streams_finished =
            std::all_of(sources.begin(), sources.end(),
                [](const std::shared_ptr<rtc::WavFileSource>& source) {
            return source->FileHasEnded();
        });
    }

    return 0;
}

*/