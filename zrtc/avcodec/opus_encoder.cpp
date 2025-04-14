#include "opus_encoder.h"
#include "av_common.h"

OpusAudioEncoder::OpusAudioEncoder()
{

}

OpusAudioEncoder::~OpusAudioEncoder()
{

}

bool OpusAudioEncoder::Init(AVConfig av_config)
{
	if (encoder_) {
		return false;
	}

	int error = 0;
	encoder_ = opus_encoder_create(samplerate_, channels_, OPUS_APPLICATION_AUDIO, &error);
	if (error != OPUS_OK || encoder_ == NULL) {
		LOG("cannot create encoder: %s\n", opus_strerror(error));
		return false;
	}

	opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(bitrate_bps_));
	opus_encoder_ctl(encoder_, OPUS_SET_BANDWIDTH(bandwidth_));
	opus_encoder_ctl(encoder_, OPUS_SET_VBR(use_vbr_));
	opus_encoder_ctl(encoder_, OPUS_SET_VBR_CONSTRAINT(use_cbr_));
	opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(complexity_));
	opus_encoder_ctl(encoder_, OPUS_SET_INBAND_FEC(use_inbandfec_));
	opus_encoder_ctl(encoder_, OPUS_SET_FORCE_CHANNELS(forcechannels_));
	//opus_encoder_ctl(encoder_, OPUS_SET_DTX(use_dtx_));
	opus_encoder_ctl(encoder_, OPUS_SET_PACKET_LOSS_PERC(packet_loss_perc_));
	opus_encoder_ctl(encoder_, OPUS_GET_LOOKAHEAD(&skip_));
	opus_encoder_ctl(encoder_, OPUS_SET_LSB_DEPTH(16));
	opus_encoder_ctl(encoder_, OPUS_SET_EXPERT_FRAME_DURATION(variable_duration_));
	opus_encoder_ctl(encoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));

	return true;
}

void OpusAudioEncoder::Destroy()
{
	if (encoder_) {
		opus_encoder_destroy(encoder_);
		encoder_ = nullptr;
	}
}

std::vector<uint8_t> OpusAudioEncoder::Encode(int16_t* pcm, int samples)
{
	std::vector<uint8_t> encode_frame;
	if (!encoder_) {
		return encode_frame;
	}

	int max_data_bytes = 1500;
	uint8_t encoder_buffer[1500] = {0};
	int len = opus_encode(encoder_, pcm, samples, encoder_buffer, max_data_bytes);
	//int bytes = opus_packet_get_samples_per_frame(encoder_buffer, samplerate_) * opus_packet_get_nb_frames(encoder_buffer, len);
	
	if (len > 0) {
		encode_frame.assign(encoder_buffer, encoder_buffer + len);
	}
	return encode_frame;
}