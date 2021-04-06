#pragma once

#include "video_encoder.h"
#include "mfxvideo++.h"
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace xop {

class IntelD3DEncoder : public VideoEncoder
{
public:
	IntelD3DEncoder();
	virtual ~IntelD3DEncoder();

	static bool IsSupported();

	virtual bool Init() override;
	virtual void Destroy()override;

	virtual int  Encode(std::vector<uint8_t> image, std::vector<uint8_t>& out_frame);

private:
	bool UpdateOption();
	bool UpdateEvent();
	bool AllocateSurfaces();
	void FreeSurface();
	bool AllocateBuffer();
	void FreeBuffer();
	bool GetVideoParam();
	int  CopyImage(std::vector<uint8_t>& in_image);
	int  EncodeFrame(int suface_index, std::vector<uint8_t>& out_frame);

	bool use_d3d11_ = false;
	bool use_d3d9_ = false;

	int gpu_index_ = 0;
	int width_ = 1920;
	int height_ = 1080;
	int bitrate_kbps_ = 8000;
	int frame_rate_ = 30;
	int gop_ = 300;
	int codec_ = 1;
	int dxgi_format_ = 87;

	mfxIMPL                mfx_impl_;
	mfxVersion             mfx_ver_;
	MFXVideoSession        mfx_session_;
	mfxFrameAllocator      mfx_allocator_;
	mfxVideoParam          mfx_enc_params_;
	mfxVideoParam          mfx_video_params_;
	mfxFrameAllocResponse  mfx_alloc_response_;
	mfxExtCodingOption     extended_coding_options_;
	mfxExtCodingOption2    extended_coding_options2_;
	mfxExtBuffer* extended_buffers_[2];
	mfxEncodeCtrl          enc_ctrl_;

	std::unique_ptr<MFXVideoENCODE> mfx_encoder_;

	mfxBitstream           mfx_enc_bs_;
	std::vector<mfxU8>     bst_enc_data_;
	std::vector<mfxFrameSurface1> mfx_surfaces_;

	mfxU16 sps_size_ = 0;
	mfxU16 pps_size_ = 0;
	std::unique_ptr<mfxU8[]> sps_buffer_;
	std::unique_ptr<mfxU8[]> pps_buffer_;
};

}
