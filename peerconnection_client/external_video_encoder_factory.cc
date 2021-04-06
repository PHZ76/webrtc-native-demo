#include "external_video_encoder_factory.h"
#include "absl/memory/memory.h"
#include "media/engine/internal_decoder_factory.h"
#include "rtc_base/logging.h"
#include "absl/strings/match.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"

#include "nv_encoder.h"
#include "qsv_encoder.h"

namespace webrtc {

class ExternalEncoderFactory : public webrtc::VideoEncoderFactory {
public:
	std::vector<webrtc::SdpVideoFormat> GetSupportedFormats()
		const override {
		std::vector<webrtc::SdpVideoFormat> video_formats;
		for (const webrtc::SdpVideoFormat& h264_format : webrtc::SupportedH264Codecs())
			video_formats.push_back(h264_format);
		return video_formats;
	}

	VideoEncoderFactory::CodecInfo QueryVideoEncoder(
		const webrtc::SdpVideoFormat& format) const override {
		CodecInfo codec_info = { false, false };
		codec_info.is_hardware_accelerated = true;
		codec_info.has_internal_source = false;
		return codec_info;
	}

	std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
		const webrtc::SdpVideoFormat& format) override {
		if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName)) {
			if (webrtc::H264Encoder::IsSupported()) {
				if (xop::NvidiaD3D11Encoder::IsSupported()) {
					return absl::make_unique<webrtc::NvEncoder>(cricket::VideoCodec(format));
				}
				else if (xop::IntelD3DEncoder::IsSupported()) {
					return absl::make_unique<webrtc::QsvEncoder>(cricket::VideoCodec(format));
				}
				else {
					return webrtc::H264Encoder::Create(cricket::VideoCodec(format));
				}
			}			
		}

		return nullptr;
	}
};

std::unique_ptr<webrtc::VideoEncoderFactory> CreateBuiltinExternalVideoEncoderFactory() {
	return absl::make_unique<ExternalEncoderFactory>();
}

}


