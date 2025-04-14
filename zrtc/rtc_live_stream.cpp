#include "rtc_live_stream.h"
#include "rtc/rtc_log.h"
#include "rtc/h264_parser.h"
#include "net/Timestamp.h"
#include "net/Timer.h"

static const uint8_t H264_FRAME_TYPE_IDR = 5;
static const uint8_t H264_FRAME_TYPE_SPS = 7;
static const uint8_t H264_FRAME_TYPE_PPS = 8;
static const uint8_t H264_FRAME_TYPE_REF = 1;

static const uint32_t RTC_OPUS_FRAME_SAMPLES = 480; // 10ms
static const uint32_t RTC_OPUS_SAMPLE_RATE = 48000;
static const uint32_t RTC_OPUS_CHANNEL = 2;

RtcLiveStream::RtcLiveStream()
{

}

RtcLiveStream::~RtcLiveStream()
{
	Destroy();
}

bool RtcLiveStream::Init()
{
	InitVideo();
	InitAudio();

	return true;
}

void RtcLiveStream::Destroy()
{
	if (video_thread_) {
		start_video_ = false;
		video_thread_->join();
		video_thread_ = nullptr;
	}

	if (audio_thread_) {
		start_audio_ = false;
		audio_thread_->join();
		audio_thread_ = nullptr;
		resampler_ = nullptr;
	}

	memset(&video_config_, 0, sizeof(video_config_));
	memset(&audio_config_, 0, sizeof(audio_config_));
}

void RtcLiveStream::SetVideoCallback(const VideoCallback& callback)
{
	video_callback_ = callback;
}

void RtcLiveStream::SetAudioCallback(const AudioCallback& callback)
{
	audio_callback_ = callback;
}

bool RtcLiveStream::InitVideo()
{
	if (video_thread_) {
		return false;
	}

	screen_capture_ = std::make_shared<DX::D3D11ScreenCapture>();
	if (!screen_capture_->Init()) {
		RTC_LOG_ERROR("init dx11 failed.");
		return false;
	}

	DX::Image image;
	if (!screen_capture_->Capture(image)) {
		RTC_LOG_ERROR("capture image failed.");
		return false;
	}

	h264_encoder_ = std::make_shared<ffmpeg::H264Encoder>();
	video_config_.video.framerate = 25;
	video_config_.video.bitrate = 2000000;
	video_config_.video.gop = video_config_.video.framerate;
	video_config_.video.format = AV_PIX_FMT_BGRA;
	video_config_.video.width = image.width;
	video_config_.video.height = image.height;

	if (!h264_encoder_->Init(video_config_)) {
		RTC_LOG_ERROR("init h264 encoder failed.");
		return false;
	}

	video_thread_.reset(new std::thread([this] {
		start_video_ = true;
		int64_t frame_interval = 1000 / video_config_.video.framerate;
		while (start_video_) {
			xop::Timestamp timestamp;
			CaptureVideo();

			int64_t capture_interval = timestamp.Elapsed();
			int64_t duration = frame_interval > capture_interval ? frame_interval - capture_interval : 1;
			xop::Timer::Sleep(duration);
		}
	}));

	return true;
}

void RtcLiveStream::CaptureVideo()
{
	if (!video_callback_) {
		return;
	}

	DX::Image image;
	if (!screen_capture_->Capture(image)) {
		return;
	}

	auto packet = h264_encoder_->Encode(image.bgra.data(), image.width, image.height, (uint32_t)image.bgra.size());
	if (!packet) {
		return;
	}

	uint8_t nalu_header = packet->data[4];
	uint8_t nalu_type = nalu_header & 0x1f;
	if (nalu_type == H264_FRAME_TYPE_IDR) {
		uint8_t* extra_data = h264_encoder_->GetAVCodecContext()->extradata;
		int extra_data_size = h264_encoder_->GetAVCodecContext()->extradata_size;
		auto sps = H264Parser::find_nalu(extra_data, extra_data_size);
		if (sps.first == nullptr || sps.second == nullptr) {
			return;
		}
		uint32_t sps_size = static_cast<uint32_t>(sps.second - sps.first + 1);
		video_callback_(sps.first, sps_size, H264_FRAME_TYPE_SPS);

		auto pps = H264Parser::find_nalu(sps.second, static_cast<uint32_t>(extra_data_size - (sps.second - extra_data)));
		if (pps.first == nullptr || pps.second == nullptr) {
			return;
		}
		uint32_t pps_size = static_cast<uint32_t>(pps.second - pps.first + 1);
		video_callback_(pps.first, pps_size, H264_FRAME_TYPE_PPS);

		video_callback_(packet->data, packet->size, H264_FRAME_TYPE_IDR);
	}
	else {
		video_callback_(packet->data, packet->size, H264_FRAME_TYPE_REF);
	}
}

bool RtcLiveStream::InitAudio()
{
	if (audio_thread_) {
		return false;
	}

	audio_capture_ = std::make_shared<AudioCapture>();
	if (!audio_capture_->Init()) {
		RTC_LOG_ERROR("init wasapi failed.");
		return false;
	}

	uint32_t input_samplerate = audio_capture_->GetSamplerate();
	uint32_t input_channels = audio_capture_->GetChannels();
	RTC_LOG_INFO("wasapi samplerate:{} channels:{}", input_samplerate, input_channels);

	opus_encoder_ = std::make_shared<OpusAudioEncoder>();
	audio_config_.audio.bitrate = 128000;
	audio_config_.audio.channels = RTC_OPUS_CHANNEL;
	audio_config_.audio.samplerate = RTC_OPUS_SAMPLE_RATE;
	audio_config_.audio.format = AV_SAMPLE_FMT_S16;

	if (input_samplerate != RTC_OPUS_SAMPLE_RATE || input_channels != RTC_OPUS_CHANNEL) {
		resampler_ = std::make_shared<ffmpeg::Resampler>();
		if (!resampler_->Init(input_samplerate, input_channels, AV_SAMPLE_FMT_S16,
			RTC_OPUS_SAMPLE_RATE, RTC_OPUS_CHANNEL, AV_SAMPLE_FMT_S16)) {
			RTC_LOG_ERROR("init resampler failed.");
			resampler_ = nullptr;
		}
	}

	if (!opus_encoder_->Init(audio_config_)) {
		RTC_LOG_ERROR("init opus encoder failed.");
		return false;
	}

	audio_thread_.reset(new std::thread([this] {
		start_audio_ = true;
		while (start_audio_) {
			CaptureAudio();
			xop::Timer::Sleep(1);
		}
	}));

	return true;
}

void RtcLiveStream::CaptureAudio()
{
	if (!audio_callback_) {
		return;
	}

	std::shared_ptr<uint8_t> pcm_buffer(new uint8_t[RTC_OPUS_FRAME_SAMPLES * RTC_OPUS_CHANNEL * 10]);
	int samples = audio_capture_->Read(pcm_buffer.get(), RTC_OPUS_FRAME_SAMPLES);
	if (samples <= 0) {
		return;
	}

	if (!resampler_) {
		auto opus_frame = opus_encoder_->Encode((int16_t*)pcm_buffer.get(), samples);
		if (opus_frame.size() > 0) {
			audio_callback_(opus_frame.data(), opus_frame.size());
		}
	}
	else {
		ffmpeg::AVFramePtr in_frame(av_frame_alloc(), [](AVFrame* ptr) { av_frame_free(&ptr); });
		in_frame->sample_rate = audio_capture_->GetSamplerate();
		in_frame->format = AV_SAMPLE_FMT_S16;
		in_frame->channels = audio_capture_->GetChannels();
		in_frame->channel_layout = av_get_default_channel_layout(audio_capture_->GetChannels());
		in_frame->nb_samples = samples;

		if (av_frame_get_buffer(in_frame.get(), 0) < 0) {
			LOG("av_frame_get_buffer() failed.\n");
			return ;
		}

		int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
		if (bytes_per_sample == 0) {
			return ;
		}

		memcpy(in_frame->data[0], pcm_buffer.get(), bytes_per_sample * in_frame->channels * samples);

		ffmpeg::AVFramePtr out_frame = nullptr;
		if (resampler_->Convert(in_frame, out_frame) <= 0) {
			return ;
		}

		auto opus_frame = opus_encoder_->Encode((int16_t*)out_frame->data[0], out_frame->nb_samples);
		if (opus_frame.size() > 0) {
			audio_callback_(opus_frame.data(), opus_frame.size());
		}
	}
}
