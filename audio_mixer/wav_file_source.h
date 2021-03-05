#pragma once

#include "api/audio/audio_mixer.h"
#include "common_audio/wav_file.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_mixer/default_output_rate_calculator.h"

#include <string>

namespace rtc {

class WavFileSource : public webrtc::AudioMixer::Source {
public:
    WavFileSource(std::string filename, int ssrc = -1);

    AudioFrameInfo GetAudioFrameWithInfo(int target_rate_hz, webrtc::AudioFrame* frame) override;

    int Ssrc() const override;

    int PreferredSampleRate() const override;

    bool FileHasEnded() const;

private:
    std::unique_ptr<webrtc::WavReader> wav_reader_;
    int sample_rate_hz_;
    int samples_per_channel_;
    int number_of_channels_;
    int ssrc_;
    bool file_has_ended_ = false;
};

}

