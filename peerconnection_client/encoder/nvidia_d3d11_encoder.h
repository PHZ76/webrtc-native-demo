#pragma once

#include "video_encoder.h"
#include "NvEncoder/NvEncoderD3D11.h"
#include <dxgi.h>
#include <d3d11.h>

namespace xop {

class NvidiaD3D11Encoder : public VideoEncoder
{
public:
	NvidiaD3D11Encoder();
	virtual ~NvidiaD3D11Encoder();

	static bool IsSupported();

	virtual bool Init() override;
	virtual void Destroy()override;

	virtual int  Encode(std::vector<uint8_t> in_image, std::vector<uint8_t>& out_frame);

	virtual int  Encode(HANDLE shared_handle, std::vector<uint8_t>& out_frame);

private:
	bool UpdateOption();
	void UpdateEvent();
	bool InitD3D11();
	void ClearD3D11();

	ID3D11Device* d3d11_device_              = nullptr;
	ID3D11DeviceContext* d3d11_context_      = nullptr;
	ID3D11Texture2D* d3d11_copy_texture_     = nullptr;
	IDXGIAdapter* d3d11_adapter_             = nullptr;
	IDXGIFactory1* d3d11_factory_            = nullptr;

	int gpu_index_     = 0;
	int width_         = 1920;
	int height_        = 1080;
	int bitrate_kbps_  = 8000;
	int frame_rate_    = 30;
	int gop_           = 300;
	int dxgi_format_   = 87;
	int codec_         = 1;

	NV_ENC_BUFFER_FORMAT nv_buffer_format_ = NV_ENC_BUFFER_FORMAT_ARGB;
	GUID nv_codec_id_ = NV_ENC_CODEC_H264_GUID;
	NvEncoderD3D11* nv_encoder_ = nullptr;
};

}
