#include "intel_d3d_encoder.h"
#include "common_utils.h"
#include "libyuv.h"
#include <Windows.h>
#include <versionhelpers.h>

#define LOG(format, ...)  	\
{								\
    fprintf(stderr, "[%s:%s:%d] " format "", \
			__FILE__, __FUNCTION__ ,  \
			__LINE__, ##__VA_ARGS__); \
}

namespace xop
{

IntelD3DEncoder::IntelD3DEncoder()
	: sps_buffer_(new mfxU8[1024])
	, pps_buffer_(new mfxU8[1024])
{
	mfx_impl_ = MFX_IMPL_AUTO_ANY;
	mfx_ver_ = { {0, 1} };

	mfxIMPL impl = mfx_impl_;

	if (IsWindows8OrGreater()) {
		use_d3d11_ = true;
		impl = mfx_impl_ | MFX_IMPL_VIA_D3D11;
	}
	else {
		impl = mfx_impl_ | MFX_IMPL_VIA_D3D9;
		use_d3d9_ = true;
	}

	mfxStatus sts = MFX_ERR_NONE;
	sts = mfx_session_.Init(impl, &mfx_ver_);
	if (sts == MFX_ERR_NONE) {
		//mfx_session_.QueryVersion(&mfx_ver_);
		//mfx_session_.Close();
	}
}

IntelD3DEncoder::~IntelD3DEncoder()
{
	Destroy();
}

bool IntelD3DEncoder::IsSupported()
{
	mfxVersion             mfx_ver;
	MFXVideoSession        mfx_session;
	mfxIMPL                mfx_impl;

	mfx_impl = MFX_IMPL_AUTO_ANY;
	mfx_ver = { {0, 1} };

	if (IsWindows8OrGreater()) {
		mfx_impl |= MFX_IMPL_VIA_D3D11;
	}
	else {
		mfx_impl |= MFX_IMPL_VIA_D3D9;
	}

	mfxStatus sts = MFX_ERR_NONE;
	sts = mfx_session.Init(mfx_impl, &mfx_ver);
	mfx_session.Close();
	return sts == MFX_ERR_NONE;
}

bool IntelD3DEncoder::Init()
{
	if (mfx_encoder_) {
		return false;
	}

	if (!IsSupported()) {
		return false;
	}

	if (!UpdateOption()) {
		return false;
	}

	mfxStatus sts = MFX_ERR_NONE;
	memset(&mfx_video_params_, 0, sizeof(mfxVideoParam));

	sts = Initialize(mfx_impl_, mfx_ver_, &mfx_session_, &mfx_allocator_);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	mfx_encoder_.reset(new MFXVideoENCODE(mfx_session_));

	sts = mfx_encoder_->Query(&mfx_enc_params_, &mfx_enc_params_);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
	if (sts != MFX_ERR_NONE) {
		goto failed;
	}

	sts = mfx_encoder_->Init(&mfx_enc_params_);
	if (sts != MFX_ERR_NONE) {
		goto failed;
	}

	if (!AllocateSurfaces()) {
		goto failed;
	}

	if (!AllocateBuffer()) {
		goto failed;
	}

	if (!GetVideoParam()) {
		goto failed;
	}

	return true;

failed:
	FreeBuffer();
	FreeSurface();
	mfx_encoder_.reset();
	Release();
	return false;
}

void IntelD3DEncoder::Destroy()
{
	if (mfx_encoder_) {
		FreeBuffer();
		FreeSurface();
		mfx_encoder_->Close();
		mfx_encoder_.reset();
		Release();
	}
}

int IntelD3DEncoder::Encode(std::vector<uint8_t> in_image, std::vector<uint8_t>& out_frame)
{
	if (!mfx_encoder_) {
		return -1;
	}

	if (!UpdateEvent()) {
		return -2;
	}

	int frame_index = CopyImage(in_image);
	if (frame_index < 0) {
		return -3;
	}

	int frame_size = EncodeFrame(frame_index, out_frame);
	if (frame_size < 0) {
		LOG("Encode frame failed.");
	}

	return frame_size;
}

int IntelD3DEncoder::CopyImage(std::vector<uint8_t>& in_image)
{
	mfxStatus sts = MFX_ERR_NONE;

	int index = GetFreeSurfaceIndex(mfx_surfaces_);  // Find free frame surface
	MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, index, MFX_ERR_MEMORY_ALLOC);

	// Surface locking required when read/write video surfaces
	sts = mfx_allocator_.Lock(mfx_allocator_.pthis, mfx_surfaces_[index].Data.MemId, &(mfx_surfaces_[index].Data));
	MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, index, MFX_ERR_LOCK_MEMORY);

	// load nv12 
	mfxU16 w, h, i, pitch;
	mfxU8* ptr = nullptr;
	mfxFrameInfo* info = &mfx_surfaces_[index].Info;
	mfxFrameData* data = &mfx_surfaces_[index].Data;

	pitch = data->Pitch;

	if (info->CropH > 0 && info->CropW > 0) {
		w = info->CropW;
		h = info->CropH;
	}
	else {
		w = info->Width;
		h = info->Height;
	}

	if (dxgi_format_ == VE_OPT_FORMAT_NV12) {
		ptr = data->Y + info->CropX + info->CropY * data->Pitch;
		uint8_t* data_y = in_image.data();
		uint8_t* data_uv = in_image.data() + width_ * height_;
		int stride_y = w;
		int stride_uv = w; // (width + 1) / 2;

		for (i = 0; i < h; i++) {
			memcpy(ptr + i * pitch, data_y + i * stride_y, w);
		}

		h /= 2;
		ptr = data->UV + info->CropX + (info->CropY / 2) * pitch;
		for (i = 0; i < h; i++) {
			memcpy(ptr + i * pitch, data_uv + i * stride_uv, w);
		}
	}
	else if (dxgi_format_ == VE_OPT_FORMAT_B8G8R8A8) {
		ptr = data->B + info->CropX + info->CropY * data->Pitch;
		uint8_t* data_bgra = in_image.data();
		for (int y = 0; y < h; y++) {
			memcpy(ptr + y * pitch, data_bgra + y * w * 4, w * 4);
		}
	}

	sts = mfx_allocator_.Unlock(mfx_allocator_.pthis, mfx_surfaces_[index].Data.MemId, &(mfx_surfaces_[index].Data));
	MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, index, MFX_ERR_UNKNOWN);

	return index;
}

int IntelD3DEncoder::EncodeFrame(int suface_index, std::vector<uint8_t>& out_frame)
{
	mfxSyncPoint syncp;
	mfxStatus sts = MFX_ERR_NONE;
	uint32_t frame_size = 0;

	for (;;) {
		// Encode a frame asychronously (returns immediately)
		mfxEncodeCtrl* enc_ctrl = nullptr;
		if (enc_ctrl_.FrameType) {
			enc_ctrl = &enc_ctrl_;
		}
		sts = mfx_encoder_->EncodeFrameAsync(enc_ctrl, &mfx_surfaces_[suface_index], &mfx_enc_bs_, &syncp);
		enc_ctrl_.FrameType = 0;

		if (MFX_ERR_NONE < sts && !syncp) {  // Repeat the call if warning and no output
			if (MFX_WRN_DEVICE_BUSY == sts)
				MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
		}
		else if (MFX_ERR_NONE < sts && syncp) {
			sts = MFX_ERR_NONE;     // Ignore warnings if output is available
			break;
		}
		else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
			// Allocate more bitstream buffer memory here if needed...
			break;
		}
		else {
			break;
		}
	}

	if (MFX_ERR_NONE == sts) {
		sts = mfx_session_.SyncOperation(syncp, 60000);   // Synchronize. Wait until encoded frame is ready
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		//mfx_enc_bs_.Data + mfx_enc_bs_.DataOffset, mfx_enc_bs_.DataLength
		if (mfx_enc_bs_.DataLength > 0) {
			//printf("encoder output frame size: %u \n", mfx_enc_bs_.DataLength);
			//out_frame.insert(out_frame.end(), sps_buffer_.get(), sps_buffer_.get() + sps_size_);
			//frame_size += sps_size_;
			//out_frame.insert(out_frame.end(), pps_buffer_.get(), pps_buffer_.get() + pps_size_);
			//frame_size += pps_size_;

			if (mfx_enc_params_.mfx.CodecId == MFX_CODEC_AVC) {
				out_frame.insert(out_frame.end(), mfx_enc_bs_.Data, mfx_enc_bs_.Data + mfx_enc_bs_.DataLength);
			}
			else if (mfx_enc_params_.mfx.CodecId == MFX_CODEC_HEVC) {
				out_frame.insert(out_frame.end(), mfx_enc_bs_.Data, mfx_enc_bs_.Data + mfx_enc_bs_.DataLength);
			}

			frame_size += mfx_enc_bs_.DataLength;
			mfx_enc_bs_.DataLength = 0;
		}
	}

	return frame_size;
}

bool IntelD3DEncoder::UpdateOption()
{
	gpu_index_ = GetOption(VE_OPT_GPU_INDEX, 0);
	width_ = GetOption(VE_OPT_WIDTH, 1920);
	height_ = GetOption(VE_OPT_HEIGHT, 1080);
	bitrate_kbps_ = GetOption(VE_OPT_BITRATE_KBPS, 8000);
	frame_rate_ = GetOption(VE_OPT_FRAME_RATE, 30);
	gop_ = GetOption(VE_OPT_GOP, 300);
	dxgi_format_ = GetOption(VE_OPT_TEXTURE_FORMAT, 87);
	codec_ = GetOption(VE_OPT_CODEC, VE_OPT_CODEC_H264);

	memset(&mfx_enc_params_, 0, sizeof(mfx_enc_params_));

	if (dxgi_format_ == VE_OPT_FORMAT_NV12) {
		mfx_enc_params_.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	}
	else if (dxgi_format_ == VE_OPT_FORMAT_B8G8R8A8) {
		mfx_enc_params_.mfx.FrameInfo.FourCC = MFX_FOURCC_RGB4;
	}
	else {
		LOG("Format unsupported.");
		return false;
	}

	if (codec_ == VE_OPT_CODEC_H264) {
		mfx_enc_params_.mfx.CodecId = MFX_CODEC_AVC;
		mfx_enc_params_.mfx.CodecProfile = MFX_PROFILE_AVC_BASELINE;
	}
	else if (codec_ == VE_OPT_CODEC_HEVC) {
		mfx_enc_params_.mfx.CodecId = MFX_CODEC_HEVC;
		mfx_enc_params_.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
	}
	else {
		LOG("Codec unsupported.\n");
		return false;
	}

	mfx_enc_params_.mfx.GopOptFlag = MFX_GOP_STRICT;
	mfx_enc_params_.mfx.NumSlice = 1;
	// MFX_TARGETUSAGE_BEST_SPEED; MFX_TARGETUSAGE_BALANCED
	mfx_enc_params_.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
	mfx_enc_params_.mfx.FrameInfo.FrameRateExtN = frame_rate_;
	mfx_enc_params_.mfx.FrameInfo.FrameRateExtD = 1;
	mfx_enc_params_.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	mfx_enc_params_.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	mfx_enc_params_.mfx.FrameInfo.CropX = 0;
	mfx_enc_params_.mfx.FrameInfo.CropY = 0;
	mfx_enc_params_.mfx.FrameInfo.CropW = width_;
	mfx_enc_params_.mfx.FrameInfo.CropH = height_;
	mfx_enc_params_.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
	mfx_enc_params_.mfx.TargetKbps = bitrate_kbps_;
	mfx_enc_params_.mfx.GopPicSize = static_cast<mfxU16>(gop_); 
	mfx_enc_params_.mfx.IdrInterval = static_cast<mfxU16>(gop_);

	// Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a
	// multiple of 32 in case of field picture
	mfx_enc_params_.mfx.FrameInfo.Width = MSDK_ALIGN16(width_);
	mfx_enc_params_.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == mfx_enc_params_.mfx.FrameInfo.PicStruct) 
											? MSDK_ALIGN16(height_) 
											: MSDK_ALIGN32(height_);

	// d3d11 or d3d9
	mfx_enc_params_.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

	// Configuration for low latency
	mfx_enc_params_.AsyncDepth = 1;  //1 is best for low latency
	mfx_enc_params_.mfx.GopRefDist = 1; //1 is best for low latency, I and P frames only

	memset(&extended_coding_options_, 0, sizeof(mfxExtCodingOption));
	extended_coding_options_.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
	extended_coding_options_.Header.BufferSz = sizeof(mfxExtCodingOption);
	//option.RefPicMarkRep = MFX_CODINGOPTION_ON;
	extended_coding_options_.NalHrdConformance = MFX_CODINGOPTION_OFF;
	extended_coding_options_.PicTimingSEI = MFX_CODINGOPTION_OFF;
	extended_coding_options_.AUDelimiter = MFX_CODINGOPTION_OFF;
	extended_coding_options_.MaxDecFrameBuffering = 1;

	memset(&extended_coding_options2_, 0, sizeof(mfxExtCodingOption2));
	extended_coding_options2_.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
	extended_coding_options2_.Header.BufferSz = sizeof(mfxExtCodingOption2);
	extended_coding_options2_.RepeatPPS = MFX_CODINGOPTION_OFF;

	extended_buffers_[0] = (mfxExtBuffer*)(&extended_coding_options_);
	extended_buffers_[1] = (mfxExtBuffer*)(&extended_coding_options2_);
	mfx_enc_params_.ExtParam = extended_buffers_;
	mfx_enc_params_.NumExtParam = 2;

	return true;
}

bool IntelD3DEncoder::UpdateEvent()
{
	if (!mfx_encoder_) {
		return false;
	}

	std::map<int, int> encoder_events = GetEvent();
	if (encoder_events.empty()) {
		return true;
	}

	bool config_updated = false;
	mfxVideoParam video_param;
	memset(&video_param, 0, sizeof(mfxVideoParam));
	mfxStatus status = mfx_encoder_->GetVideoParam(&video_param);
	MSDK_IGNORE_MFX_STS(status, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
	if (status != MFX_ERR_NONE) {
		return false;
	}

	for (auto iter : encoder_events) {
		int event = iter.first;
		int value = iter.second;

		switch (event)
		{
		case VE_EVENT_FORCE_IDR:
			enc_ctrl_.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
			break;

		case VE_EVENT_RESET_BITRATE_KBPS:
			config_updated = true;
			video_param.mfx.TargetKbps= value;
			break;

		case VE_EVENT_RESET_FRAME_RATE:
			config_updated = true;
			video_param.mfx.FrameInfo.FrameRateExtN = value;
			break;

		default:
			break;
		}
	}

	if (config_updated) {
		if (status == MFX_ERR_NONE) {
			status = mfx_encoder_->Reset(&video_param);
			MSDK_IGNORE_MFX_STS(status, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
			if (status != MFX_ERR_NONE) {
				return false;
			}
		}
	}

	return true;
}

bool IntelD3DEncoder::AllocateSurfaces()
{
	mfxStatus sts = MFX_ERR_NONE;

	// Query number of required surfaces for encoder
	mfxFrameAllocRequest enc_request;
	memset(&enc_request, 0, sizeof(mfxFrameAllocRequest));
	sts = mfx_encoder_->QueryIOSurf(&mfx_enc_params_, &enc_request);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	// This line is only required for Windows DirectX11 to ensure that surfaces can be written to by the application
	enc_request.Type |= WILL_WRITE;

	// Allocate required surfaces
	sts = mfx_allocator_.Alloc(mfx_allocator_.pthis, &enc_request, &mfx_alloc_response_);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	mfxU16 num_surfaces = mfx_alloc_response_.NumFrameActual;

	// Allocate surface headers (mfxFrameSurface1) for encoder
	mfx_surfaces_.resize(num_surfaces);
	for (int i = 0; i < num_surfaces; i++) {
		memset(&mfx_surfaces_[i], 0, sizeof(mfxFrameSurface1));
		mfx_surfaces_[i].Info = mfx_enc_params_.mfx.FrameInfo;
		mfx_surfaces_[i].Data.MemId = mfx_alloc_response_.mids[i];
		ClearYUVSurfaceVMem(mfx_surfaces_[i].Data.MemId);
	}

	return true;
}

void IntelD3DEncoder::FreeSurface()
{
	if (mfx_alloc_response_.NumFrameActual > 0) {
		mfx_allocator_.Free(mfx_allocator_.pthis, &mfx_alloc_response_);
		memset(&mfx_alloc_response_, 0, sizeof(mfxFrameAllocResponse));
	}
}

bool IntelD3DEncoder::AllocateBuffer()
{
	mfxStatus sts = MFX_ERR_NONE;
	mfxVideoParam param;
	memset(&param, 0, sizeof(mfxVideoParam));
	sts = mfx_encoder_->GetVideoParam(&param);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	memset(&mfx_enc_bs_, 0, sizeof(mfxBitstream));
	mfx_enc_bs_.MaxLength = param.mfx.BufferSizeInKB * 1000;
	bst_enc_data_.resize(mfx_enc_bs_.MaxLength);
	mfx_enc_bs_.Data = bst_enc_data_.data();
	return true;
}

void IntelD3DEncoder::FreeBuffer()
{
	memset(&mfx_enc_bs_, 0, sizeof(mfxBitstream));
	bst_enc_data_.clear();
}

bool IntelD3DEncoder::GetVideoParam()
{
	mfxExtCodingOptionSPSPPS sps_pps_opt;
	mfxExtBuffer* extended_buffers[1];

	sps_pps_opt.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
	sps_pps_opt.Header.BufferSz = sizeof(mfxExtCodingOptionSPSPPS);
	extended_buffers[0] = (mfxExtBuffer*)&sps_pps_opt;

	mfx_video_params_.ExtParam = extended_buffers;
	mfx_video_params_.NumExtParam = 1;
	sps_pps_opt.SPSBuffer = sps_buffer_.get();
	sps_pps_opt.PPSBuffer = pps_buffer_.get();
	sps_pps_opt.SPSBufSize = 1024;
	sps_pps_opt.PPSBufSize = 1024;
	mfxStatus sts = mfx_encoder_->GetVideoParam(&mfx_video_params_);
	if (sts != MFX_ERR_NONE) {
		return false;
	}
	sps_size_ = sps_pps_opt.SPSBufSize;
	pps_size_ = sps_pps_opt.PPSBufSize;
	return true;
}

}

