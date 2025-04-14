#pragma once

#include "av_encoder.h"
#include "opus.h"
#include "opus_defines.h"
#include <vector>

class OpusAudioEncoder
{
public:
	OpusAudioEncoder();
	virtual ~OpusAudioEncoder();

	bool Init(AVConfig av_config);
	void Destroy();

	std::vector<uint8_t> Encode(int16_t* pcm, int samples);

private:
	::OpusEncoder* encoder_ = nullptr;

	int samplerate_ = 48000;
	int channels_ = 2;
	int bitrate_bps_ = 16000 * 8;
	int bandwidth_ = OPUS_BANDWIDTH_FULLBAND;
	int use_vbr_ = 0;
	int use_cbr_ = 1;
	int use_cvbr_ = 0;
	int complexity_ = 10;
	int use_inbandfec_ = 1;
	int forcechannels_ = OPUS_AUTO;
	int use_dtx_ = 0;
	int packet_loss_perc_ = 5;
	int skip_ = 0;
	int variable_duration_ = OPUS_FRAMESIZE_10_MS;
};
