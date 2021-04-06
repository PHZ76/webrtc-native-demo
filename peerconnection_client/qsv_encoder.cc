#include "qsv_encoder.h"

#include <limits>
#include <string>

#include "third_party/openh264/src/codec/api/svc/codec_api.h"
#include "third_party/openh264/src/codec/api/svc/codec_app_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_ver.h"
#include "absl/strings/match.h"
#include "common_video/h264/h264_common.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "modules/video_coding/utility/simulcast_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/metrics.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace webrtc {

namespace {

const bool kOpenH264EncoderDetailedLogging = false;

// QP scaling thresholds.
static const int kLowH264QpThreshold = 24;
static const int kHighH264QpThreshold = 37;

// Used by histograms. Values of entries should not be changed.
enum NvVideoEncoderEvent
{
	kH264EncoderEventInit = 0,
	kH264EncoderEventError = 1,
	kH264EncoderEventMax = 16,
};

int NumberOfThreads(int width, int height, int number_of_cores)
{
	// TODO(hbos): In Chromium, multiple threads do not work with sandbox on Mac,
	// see crbug.com/583348. Until further investigated, only use one thread.
	//  if (width * height >= 1920 * 1080 && number_of_cores > 8) {
	//    return 8;  // 8 threads for 1080p on high perf machines.
	//  } else if (width * height > 1280 * 960 && number_of_cores >= 6) {
	//    return 3;  // 3 threads for 1080p.
	//  } else if (width * height > 640 * 480 && number_of_cores >= 3) {
	//    return 2;  // 2 threads for qHD/HD.
	//  } else {
	//    return 1;  // 1 thread for VGA or less.
	//  }
	// TODO(sprang): Also check sSliceArgument.uiSliceNum om GetEncoderPrams(),
	//               before enabling multithreading here.
	return 1;
}

VideoFrameType ConvertToVideoFrameType(EVideoFrameType type) {
	switch (type) {
	case videoFrameTypeIDR:
		return VideoFrameType::kVideoFrameKey;
	case videoFrameTypeSkip:
	case videoFrameTypeI:
	case videoFrameTypeP:
	case videoFrameTypeIPMixed:
		return VideoFrameType::kVideoFrameDelta;
	case videoFrameTypeInvalid:
		break;
	}
	RTC_NOTREACHED() << "Unexpected/invalid frame type: " << type;
	return VideoFrameType::kEmptyFrame;
}

}  // namespace

static void RtpFragmentize(EncodedImage* encoded_image,
	const VideoFrameBuffer& frame_buffer,
	std::vector<uint8_t>& frame_packet,
	RTPFragmentationHeader* frag_header)
{
	size_t required_capacity = 0;
	encoded_image->set_size(0);

	required_capacity = frame_packet.size();
	encoded_image->SetEncodedData(EncodedImageBuffer::Create(required_capacity));

	// TODO(nisse): Use a cache or buffer pool to avoid allocation?
	encoded_image->SetEncodedData(EncodedImageBuffer::Create(required_capacity));

	memcpy(encoded_image->data(), &frame_packet[0], frame_packet.size());

	std::vector<webrtc::H264::NaluIndex> nalus = webrtc::H264::FindNaluIndices(
		encoded_image->data(), encoded_image->size());

	size_t fragments_count = nalus.size();

	frag_header->VerifyAndAllocateFragmentationHeader(fragments_count);

	for (size_t i = 0; i < nalus.size(); i++) {
		frag_header->fragmentationOffset[i] = nalus[i].payload_start_offset;
		frag_header->fragmentationLength[i] = nalus[i].payload_size;
	}
}

QsvEncoder::QsvEncoder(const cricket::VideoCodec& codec)
	: packetization_mode_(H264PacketizationMode::SingleNalUnit),
	max_payload_size_(0),
	number_of_cores_(0),
	encoded_image_callback_(nullptr),
	has_reported_init_(false),
	has_reported_error_(false),
	num_temporal_layers_(1),
	tl0sync_limit_(0)
{
	RTC_CHECK(absl::EqualsIgnoreCase(codec.name, cricket::kH264CodecName));
	std::string packetization_mode_string;
	if (codec.GetParam(cricket::kH264FmtpPacketizationMode, &packetization_mode_string)
		&& packetization_mode_string == "1") {
		packetization_mode_ = H264PacketizationMode::NonInterleaved;
	}

	encoded_images_.reserve(kMaxSimulcastStreams);
	qsv_encoders_.reserve(kMaxSimulcastStreams);
	configurations_.reserve(kMaxSimulcastStreams);
	image_buffer_ = nullptr;
}

QsvEncoder::~QsvEncoder()
{
	Release();
}

int32_t QsvEncoder::InitEncode(const VideoCodec* inst,
	int32_t number_of_cores,
	size_t max_payload_size)
{
	ReportInit();
	if (!inst || inst->codecType != kVideoCodecH264) {
		ReportError();
		return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
	}
	if (inst->maxFramerate == 0) {
		ReportError();
		return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
	}
	if (inst->width < 1 || inst->height < 1) {
		ReportError();
		return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
	}

	int32_t release_ret = Release();
	if (release_ret != WEBRTC_VIDEO_CODEC_OK) {
		ReportError();
		return release_ret;
	}

	int number_of_streams = SimulcastUtility::NumberOfSimulcastStreams(*inst);
	bool doing_simulcast = (number_of_streams > 1);

	if (doing_simulcast && !SimulcastUtility::ValidSimulcastParameters(*inst, number_of_streams)) {
		return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
	}

	assert(number_of_streams == 1);

	encoded_images_.resize(number_of_streams);
	qsv_encoders_.resize(number_of_streams);
	configurations_.resize(number_of_streams);

	number_of_cores_ = number_of_cores;
	max_payload_size_ = max_payload_size;
	codec_ = *inst;

	// Code expects simulcastStream resolutions to be correct, make sure they are
	// filled even when there are no simulcast layers.
	if (codec_.numberOfSimulcastStreams == 0) {
		codec_.simulcastStream[0].width = codec_.width;
		codec_.simulcastStream[0].height = codec_.height;
	}

	num_temporal_layers_ = codec_.H264()->numberOfTemporalLayers;

	for (int i = 0, idx = number_of_streams - 1; i < number_of_streams; ++i, --idx) {
		// Store nvidia encoder.
		xop::IntelD3DEncoder* qsv_encoder = new xop::IntelD3DEncoder();
		qsv_encoders_[i] = qsv_encoder;

		// Set internal settings from codec_settings
		configurations_[i].simulcast_idx = idx;
		configurations_[i].sending = false;
		configurations_[i].width = codec_.simulcastStream[idx].width;
		configurations_[i].height = codec_.simulcastStream[idx].height;
		configurations_[i].max_frame_rate = static_cast<float>(codec_.maxFramerate);
		configurations_[i].frame_dropping_on = codec_.H264()->frameDroppingOn;
		configurations_[i].key_frame_interval = codec_.H264()->keyFrameInterval;

		// Codec_settings uses kbits/second; encoder uses bits/second.
		configurations_[i].max_bps = codec_.maxBitrate * 1000;
		configurations_[i].target_bps = codec_.maxBitrate * 1000 / 2;

		qsv_encoder->SetOption(xop::VE_OPT_WIDTH, configurations_[i].width);
		qsv_encoder->SetOption(xop::VE_OPT_HEIGHT, configurations_[i].height);
		qsv_encoder->SetOption(xop::VE_OPT_FRAME_RATE, static_cast<int>(configurations_[i].max_frame_rate));
		qsv_encoder->SetOption(xop::VE_OPT_GOP, configurations_[i].key_frame_interval);
		qsv_encoder->SetOption(xop::VE_OPT_CODEC, xop::VE_OPT_CODEC_H264);
		qsv_encoder->SetOption(xop::VE_OPT_BITRATE_KBPS, configurations_[i].target_bps / 1000);
		qsv_encoder->SetOption(xop::VE_OPT_TEXTURE_FORMAT, xop::VE_OPT_FORMAT_NV12);
		if (!qsv_encoder->Init()) {
			Release();
			ReportError();
			return WEBRTC_VIDEO_CODEC_ERROR;
		}

		image_buffer_.reset(new uint8_t[configurations_[i].width * configurations_[i].height * 10]);

		// TODO(pbos): Base init params on these values before submitting.
		video_format_ = EVideoFormatType::videoFormatI420;

		// Initialize encoded image. Default buffer size: size of unencoded data.
		const size_t new_capacity = CalcBufferSize(VideoType::kI420,
			codec_.simulcastStream[idx].width, codec_.simulcastStream[idx].height);
		encoded_images_[i].SetEncodedData(EncodedImageBuffer::Create(new_capacity));
		encoded_images_[i]._completeFrame = true;
		encoded_images_[i]._encodedWidth = codec_.simulcastStream[idx].width;
		encoded_images_[i]._encodedHeight = codec_.simulcastStream[idx].height;
		encoded_images_[i].set_size(0);
	}

	SimulcastRateAllocator init_allocator(codec_);
	VideoBitrateAllocation allocation = init_allocator.GetAllocation(
		codec_.maxBitrate * 1000 / 2, codec_.maxFramerate);
	SetRates(RateControlParameters(allocation, codec_.maxFramerate));
	return WEBRTC_VIDEO_CODEC_OK;
}

int32_t QsvEncoder::Release()
{
	while (!qsv_encoders_.empty())
	{
		xop::IntelD3DEncoder* qsv_encoder = reinterpret_cast<xop::IntelD3DEncoder*>(qsv_encoders_.back());
		if (qsv_encoder) {
			qsv_encoder->Destroy();
			delete qsv_encoder;
		}
		qsv_encoders_.pop_back();
	}

	configurations_.clear();
	encoded_images_.clear();

	return WEBRTC_VIDEO_CODEC_OK;
}

int32_t QsvEncoder::RegisterEncodeCompleteCallback(EncodedImageCallback* callback)
{
	encoded_image_callback_ = callback;
	return WEBRTC_VIDEO_CODEC_OK;
}

void QsvEncoder::SetRates(const RateControlParameters& parameters)
{
	if (parameters.bitrate.get_sum_bps() == 0) {
		// Encoder paused, turn off all encoding.
		for (size_t i = 0; i < configurations_.size(); ++i)
			configurations_[i].SetStreamState(false);
		return;
	}

	// At this point, bitrate allocation should already match codec settings.
	if (codec_.maxBitrate > 0)
		RTC_DCHECK_LE(parameters.bitrate.get_sum_kbps(), codec_.maxBitrate);
	RTC_DCHECK_GE(parameters.bitrate.get_sum_kbps(), codec_.minBitrate);
	if (codec_.numberOfSimulcastStreams > 0)
		RTC_DCHECK_GE(parameters.bitrate.get_sum_kbps(), codec_.simulcastStream[0].minBitrate);

	codec_.maxFramerate = static_cast<uint32_t>(parameters.framerate_fps);

	size_t stream_idx = qsv_encoders_.size() - 1;
	for (size_t i = 0; i < qsv_encoders_.size(); ++i, --stream_idx) {
		configurations_[i].target_bps = parameters.bitrate.GetSpatialLayerSum(stream_idx);
		configurations_[i].max_frame_rate = static_cast<float>(parameters.framerate_fps);

		if (configurations_[i].target_bps) {
			configurations_[i].SetStreamState(true);

			if (qsv_encoders_[i]) {
				xop::IntelD3DEncoder* qsv_encoder = reinterpret_cast<xop::IntelD3DEncoder*>(qsv_encoders_[i]);
				qsv_encoder->SetEvent(xop::VE_EVENT_RESET_BITRATE_KBPS, configurations_[i].target_bps / 1000);
				qsv_encoder->SetEvent(xop::VE_EVENT_RESET_FRAME_RATE, static_cast<int>(configurations_[i].max_frame_rate));
			}
			else {
				configurations_[i].SetStreamState(false);
			}
		}
	}
}

int32_t QsvEncoder::Encode(const VideoFrame& input_frame,
	const std::vector<VideoFrameType>* frame_types)
{
	if (qsv_encoders_.empty()) {
		ReportError();
		return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
	}
	if (!encoded_image_callback_) {
		RTC_LOG(LS_WARNING)
			<< "InitEncode() has been called, but a callback function "
			<< "has not been set with RegisterEncodeCompleteCallback()";
		ReportError();
		return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
	}

	rtc::scoped_refptr<const I420BufferInterface> frame_buffer = input_frame.video_frame_buffer()->ToI420();

	bool send_key_frame = false;
	for (size_t i = 0; i < configurations_.size(); ++i) {
		if (configurations_[i].key_frame_request && configurations_[i].sending) {
			send_key_frame = true;
			break;
		}
	}
	if (!send_key_frame && frame_types) {
		for (size_t i = 0; i < frame_types->size() && i < configurations_.size(); ++i) {
			if ((*frame_types)[i] == VideoFrameType::kVideoFrameKey && configurations_[i].sending) {
				send_key_frame = true;
				break;
			}
		}
	}

	RTC_DCHECK_EQ(configurations_[0].width, frame_buffer->width());
	RTC_DCHECK_EQ(configurations_[0].height, frame_buffer->height());

	// Encode image for each layer.
	for (size_t i = 0; i < qsv_encoders_.size(); ++i) {
		if (!configurations_[i].sending) {
			continue;
		}

		if (frame_types != nullptr) {
			// Skip frame?
			if ((*frame_types)[i] == VideoFrameType::kEmptyFrame) {
				continue;
			}
		}

		if (send_key_frame) {
			if (!qsv_encoders_.empty() && qsv_encoders_[i]) {
				xop::IntelD3DEncoder* qsv_encoder = reinterpret_cast<xop::IntelD3DEncoder*>(qsv_encoders_[i]);
				qsv_encoder->SetEvent(xop::VE_EVENT_FORCE_IDR, 1);
			}

			configurations_[i].key_frame_request = false;
		}

		// EncodeFrame output.
		SFrameBSInfo info;
		memset(&info, 0, sizeof(SFrameBSInfo));
		std::vector<uint8_t> frame_packet;

		EncodeFrame((int)i, input_frame, frame_packet);

		if (frame_packet.size() == 0) {
			return WEBRTC_VIDEO_CODEC_OK;
		}
		else {
			if ((frame_packet[4] & 0x1f) == 0x07) {
				// sps + pps + idr
				info.eFrameType = videoFrameTypeIDR;
			}
			else {
				info.eFrameType = videoFrameTypeP;
			}
		}

		encoded_images_[i]._encodedWidth = configurations_[i].width;
		encoded_images_[i]._encodedHeight = configurations_[i].height;
		encoded_images_[i].SetTimestamp(input_frame.timestamp());
		encoded_images_[i].ntp_time_ms_ = input_frame.ntp_time_ms();
		encoded_images_[i].capture_time_ms_ = input_frame.render_time_ms();
		encoded_images_[i].rotation_ = input_frame.rotation();
		encoded_images_[i].SetColorSpace(input_frame.color_space());
		encoded_images_[i].content_type_ = (codec_.mode == VideoCodecMode::kScreensharing)
			? VideoContentType::SCREENSHARE
			: VideoContentType::UNSPECIFIED;
		encoded_images_[i].timing_.flags = VideoSendTiming::kInvalid;
		encoded_images_[i]._frameType = ConvertToVideoFrameType(info.eFrameType);
		encoded_images_[i].SetSpatialIndex(configurations_[i].simulcast_idx);

		// Split encoded image up into fragments. This also updates
		// |encoded_image_|.
		RTPFragmentationHeader frag_header;
		RtpFragmentize(&encoded_images_[i], *frame_buffer, frame_packet, &frag_header);

		// Encoder can skip frames to save bandwidth in which case
		// |encoded_images_[i]._length| == 0.
		if (encoded_images_[i].size() > 0) {
			// Parse QP.
			h264_bitstream_parser_.ParseBitstream(encoded_images_[i].data(),
				encoded_images_[i].size());
			h264_bitstream_parser_.GetLastSliceQp(&encoded_images_[i].qp_);

			// Deliver encoded image.
			CodecSpecificInfo codec_specific;
			codec_specific.codecType = kVideoCodecH264;
			codec_specific.codecSpecific.H264.packetization_mode = packetization_mode_;
			codec_specific.codecSpecific.H264.temporal_idx = kNoTemporalIdx;
			codec_specific.codecSpecific.H264.idr_frame = (info.eFrameType == videoFrameTypeIDR);
			codec_specific.codecSpecific.H264.base_layer_sync = false;

			// if (info.eFrameType == videoFrameTypeIDR &&
			//    encoded_images_[i]._frameType == kVideoFrameKey) {
			//  RTC_LOG(LS_ERROR) << "send idr frame - " << encoded_images_[i].size();
			//}

			encoded_image_callback_->OnEncodedImage(encoded_images_[i], &codec_specific, &frag_header);
		}
	}

	return WEBRTC_VIDEO_CODEC_OK;
}

void QsvEncoder::ReportInit()
{
	if (has_reported_init_)
		return;
	RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.QsvEncoder.Event",
		kH264EncoderEventInit, kH264EncoderEventMax);
	has_reported_init_ = true;
}

void QsvEncoder::ReportError()
{
	if (has_reported_error_)
		return;
	RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.QsvEncoder.Event",
		kH264EncoderEventError, kH264EncoderEventMax);
	has_reported_error_ = true;
}

VideoEncoder::EncoderInfo QsvEncoder::GetEncoderInfo() const
{
	EncoderInfo info;
	info.supports_native_handle = false;
	info.implementation_name = "QsvEncoder";
	info.scaling_settings = VideoEncoder::ScalingSettings(kLowH264QpThreshold, kHighH264QpThreshold);
	info.is_hardware_accelerated = true;
	info.has_internal_source = false;
	return info;
}

void QsvEncoder::LayerConfig::SetStreamState(bool send_stream)
{
	if (send_stream && !sending) {
		// Need a key frame if we have not sent this stream before.
		key_frame_request = true;
	}
	sending = send_stream;
}

bool QsvEncoder::EncodeFrame(int index, const VideoFrame& input_frame,
	std::vector<uint8_t>& frame_packet)
{
	frame_packet.clear();

	if (qsv_encoders_.empty() || !qsv_encoders_[index]) {
		return false;
	}

	if (video_format_ == EVideoFormatType::videoFormatI420) {
		if (image_buffer_ != nullptr) {
			if (webrtc::ConvertFromI420(input_frame, webrtc::VideoType::kNV12, 0,
				image_buffer_.get()) < 0) {
				return false;
			}
		}
		else {
			return false;
		}
	}

	int width = input_frame.width();
	int height = input_frame.height();
	int image_size = width * height * 3 / 2; // nv12

	xop::IntelD3DEncoder* qsv_encoder = reinterpret_cast<xop::IntelD3DEncoder*>(qsv_encoders_[index]);
	if (qsv_encoder) {
		int frame_size = qsv_encoder->Encode(std::vector<uint8_t>(image_buffer_.get(), image_buffer_.get() + image_size), frame_packet);
		if (frame_size < 0) {
			return false;
		}
	}

	return true;
}

}  // namespace webrtc
