#include "audio_mixer.h"
#include "audio_resampler.h"
#include "common_audio/wav_file.h"

#pragma comment(lib,"swresample.lib")
#pragma comment(lib,"avutil.lib")

static int total_frames = 10 * (1000 / webrtc::AudioMixerImpl::kFrameDurationInMs); // 10s

void AddFrameThread(AudioMixer* mixer, std::string file, int ssrc)
{
  
    std::unique_ptr<webrtc::WavReader> wav_reader(new webrtc::WavReader(file));
    std::unique_ptr<AudioResampler> audio_resampler(new AudioResampler());

    if (wav_reader->num_samples() < 0) {
        return;
    }

    if (!mixer->AddSource(ssrc)) {
        return;
    }

    int out_sample_rate = mixer->GetSampleRate();
    int out_channels = mixer->GetChannels();

    AVSampleFormat in_format = AV_SAMPLE_FMT_S16;
    const int in_bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    const int in_sample_rate = wav_reader->sample_rate();
    const int in_channels = static_cast<int>(wav_reader->num_channels());
    audio_resampler->Init(in_sample_rate, in_channels, in_format,
                          out_sample_rate, out_channels, AV_SAMPLE_FMT_S16);

    int max_buffer_size = in_sample_rate * in_channels * in_bytes_per_sample * 5;
    std::unique_ptr<uint8_t[]> in_buffer(new uint8_t[max_buffer_size]);
    std::unique_ptr<uint8_t[]> out_buffer(new uint8_t[max_buffer_size]);

    int frame_duration_ms = webrtc::AudioMixerImpl::kFrameDurationInMs * 2;

    while (1) {
        if (total_frames <= 0) {
            break;
        }

        memset(in_buffer.get(), 0, max_buffer_size);
        memset(out_buffer.get(), 0, max_buffer_size);

        int samples_to_read = (in_sample_rate / (1000 / frame_duration_ms)) * in_channels;
        int samples_read = (int)wav_reader->ReadSamples(samples_to_read, reinterpret_cast<int16_t*>(in_buffer.get()));
        if (samples_to_read != samples_read) {
            break;
        }

        int out_frame_size = audio_resampler->Convert(in_buffer.get(), samples_read * in_bytes_per_sample, 
                                                      out_buffer.get(), max_buffer_size);

        if (out_frame_size > 0) {
            mixer->AddFrame(ssrc, out_buffer.get(), out_frame_size);
        }
       
        Sleep(frame_duration_ms);
    }

    mixer->RemoveSource(ssrc);
}

int main(int argc, char** argv)
{
    int sample_rate = 48000;
    int num_channels = 2;   
    AudioMixer audio_mixer(sample_rate, num_channels);

    std::thread thread1([&audio_mixer] { AddFrameThread(&audio_mixer, "H:\\audio\\test1.wav", 1); });
    std::thread thread2([&audio_mixer] { AddFrameThread(&audio_mixer, "H:\\audio\\test2.wav", 2); });
    std::thread thread3([&audio_mixer] { AddFrameThread(&audio_mixer, "H:\\audio\\test3.wav", 3); });

    Sleep(1000);

    const int bytes_per_sample = audio_mixer.GetBytesPerSample();
    int max_buffer_size = bytes_per_sample * num_channels * sample_rate * 10;
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[max_buffer_size]);

    webrtc::WavWriter wav_writer("mix.wav", sample_rate, num_channels);
    while (1) {
        int frame_size = audio_mixer.MixFrame(buffer.get(), max_buffer_size);
        if (frame_size <= 0) {
            break;
        }

        total_frames -= 1;
        wav_writer.WriteSamples(reinterpret_cast<int16_t*>(buffer.get()), frame_size / bytes_per_sample);
        Sleep(webrtc::AudioMixerImpl::kFrameDurationInMs);
    }

    thread1.join();
    thread2.join();
    thread3.join();
    return 0;
}
