#ifndef MODULES_QSV_VIDEO_CODING_CODECS_H264_ENCODER_IMPL_H_
#define MODULES_QSV_VIDEO_CODING_CODECS_H264_ENCODER_IMPL_H_

#include <memory>
#include <vector>
#include <string.h>

#include "api/video/i420_buffer.h"
#include "common_video/h264/h264_bitstream_parser.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "third_party/openh264/src/codec/api/svc/codec_app_def.h"

#include <dxgi.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include "encoder/intel_d3d_encoder.h"

namespace webrtc {

class QsvEncoder : public VideoEncoder {
public:
	struct LayerConfig
	{
		int simulcast_idx = 0;
		int width = -1;
		int height = -1;
		bool sending = true;
		bool key_frame_request = false;
		float max_frame_rate = 0;
		uint32_t target_bps = 0;
		uint32_t max_bps = 0;
		bool frame_dropping_on = false;
		int key_frame_interval = 0;

		void SetStreamState(bool send_stream);
	};

public:
	explicit QsvEncoder(const cricket::VideoCodec& codec);
	~QsvEncoder() override;

	int32_t InitEncode(const VideoCodec* codec_settings,
					   int32_t number_of_cores,
					   size_t max_payload_size) override;
	int32_t Release() override;

	int32_t RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;

	void SetRates(const RateControlParameters& parameters) override;

	// The result of encoding - an EncodedImage and RTPFragmentationHeader - are
	// passed to the encode complete callback.
	int32_t Encode(const VideoFrame& frame,
				   const std::vector<VideoFrameType>* frame_types) override;

	EncoderInfo GetEncoderInfo() const override;

	// Exposed for testing.
	H264PacketizationMode PacketizationModeForTesting() const {
		return packetization_mode_;
	}

private:
	webrtc::H264BitstreamParser h264_bitstream_parser_;
	// Reports statistics with histograms.
	void ReportInit();
	void ReportError();

	bool EncodeFrame(int index, const VideoFrame& input_frame,
					 std::vector<uint8_t>& frame_packet);

	std::vector<void*> qsv_encoders_;
	std::vector<LayerConfig> configurations_;
	std::vector<EncodedImage> encoded_images_;

	VideoCodec codec_;
	H264PacketizationMode packetization_mode_;
	size_t max_payload_size_;
	int32_t number_of_cores_;
	EncodedImageCallback* encoded_image_callback_;

	bool has_reported_init_;
	bool has_reported_error_;
	int video_format_;
	int num_temporal_layers_;
	uint8_t tl0sync_limit_;

	std::unique_ptr<uint8_t[]> image_buffer_;
};

}  // namespace webrtc

#endif  // MODULES_QSV_VIDEO_CODING_CODECS_H264_ENCODER_IMPL_H_
