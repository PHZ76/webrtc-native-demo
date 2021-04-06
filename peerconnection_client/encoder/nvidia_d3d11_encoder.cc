#include "nvidia_d3d11_encoder.h"

#ifdef WIN32
#include <Windows.h>
#endif

#define LOG(format, ...)  	\
{								\
    fprintf(stderr, "[%s:%s:%d] " format "", \
			__FILE__, __FUNCTION__ ,  \
			__LINE__, ##__VA_ARGS__); \
}

namespace xop
{

class NvidiaRAII
{
public:
	NvidiaRAII() {
#if defined(_WIN64)
		module_ = LoadLibrary(TEXT("nvEncodeAPI64.dll"));
#elif defined(_WIN32)
		module_ = LoadLibrary(TEXT("nvEncodeAPI.dll"));
#endif
	}

	~NvidiaRAII() {
#ifdef WIN32
		if (module_) {
			FreeLibrary(module_);
		}
#endif
	}

#ifdef WIN32
	HMODULE GetModule() {
		return module_;
	}
#endif

private:
#ifdef WIN32
	HMODULE module_ = NULL;
#endif
};

NvidiaD3D11Encoder::NvidiaD3D11Encoder()
{

}

NvidiaD3D11Encoder::~NvidiaD3D11Encoder()
{
	Destroy();
}

bool NvidiaD3D11Encoder::IsSupported()
{
	static NvidiaRAII s_nvidia_raii;

#if defined(_WIN32)
	HMODULE module = s_nvidia_raii.GetModule();
	if (!module) {
		LOG("Module not found.");
		return false;
	}

	typedef NVENCSTATUS(NVENCAPI* APIGetVersion)(uint32_t*);
	APIGetVersion get_version_func = (APIGetVersion)GetProcAddress(module, "NvEncodeAPIGetMaxSupportedVersion");
	uint32_t version = 0;
	uint32_t current_version = (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;

	NVENC_API_CALL(get_version_func(&version));
	if (current_version > version) {
		LOG("Current Driver Version does not support this NvEncodeAPI version, please upgrade driver.");
		return false;
	}

	return true;
#else 
	return false;
#endif
}

bool NvidiaD3D11Encoder::Init()
{
	if (nv_encoder_) {
		return false;
	}

	if (!IsSupported()) {
		return false;
	}

	if (!UpdateOption()) {
		return false;
	}

	if (!InitD3D11()) {
		return false;
	}

	try {
		nv_encoder_ = new NvEncoderD3D11(d3d11_device_, width_, height_, nv_buffer_format_, 0);
	}
	catch (const NVENCException& e) {
		LOG("Failed to create nvidia encoder, %s", e.what());
		delete nv_encoder_;
		nv_encoder_ = nullptr;
		return false;
	}

	NV_ENC_INITIALIZE_PARAMS initialize_params = { NV_ENC_INITIALIZE_PARAMS_VER };
	NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
	initialize_params.encodeConfig = &encodeConfig;
	nv_encoder_->CreateDefaultEncoderParams(&initialize_params, nv_codec_id_, NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID);
	initialize_params.maxEncodeWidth = width_;
	initialize_params.maxEncodeHeight = height_;
	initialize_params.frameRateNum = frame_rate_;
	initialize_params.encodeConfig->gopLength = gop_;
	initialize_params.encodeConfig->rcParams.averageBitRate = bitrate_kbps_ * 1000;
	initialize_params.encodeConfig->rcParams.maxBitRate = bitrate_kbps_ * 1000;
	initialize_params.encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;

	try {
		nv_encoder_->CreateEncoder(&initialize_params);
		nv_encoder_->ForceIDR();
	}
	catch (const NVENCException& e) {
		LOG("Failed to init nvidia encoder, %s", e.what());
		delete nv_encoder_;
		nv_encoder_ = nullptr;
		return false;
	}

	return true;
}

void NvidiaD3D11Encoder::Destroy()
{
	if (nv_encoder_) {
		nv_encoder_->DestroyEncoder();
		delete nv_encoder_;
		nv_encoder_ = nullptr;
	}

	ClearD3D11();
}

int NvidiaD3D11Encoder::Encode(std::vector<uint8_t> in_image, std::vector<uint8_t>& out_frame)
{
	if (!nv_encoder_) {
		return -1;
	}

	UpdateEvent();
	out_frame.clear();

	D3D11_MAPPED_SUBRESOURCE dsec = { 0 };
	HRESULT hr = d3d11_context_->Map(d3d11_copy_texture_, D3D11CalcSubresource(0, 0, 0), D3D11_MAP_WRITE, 0, &dsec);

	if (FAILED(hr)) {
		return -2;
	}

	if (dxgi_format_ == VE_OPT_FORMAT_NV12) {
		uint8_t* y_plane_ = in_image.data();
		for (int i = 0; i < height_; i++) {
			memcpy((uint8_t*)dsec.pData + dsec.RowPitch * i, y_plane_ + width_ * i, width_);
		}
		uint8_t* uv_plane_ = &in_image[0] + width_ * height_;
		int h = height_ / 2;
		for (int i = 0; i < h; i++) {
			memcpy((uint8_t*)dsec.pData + dsec.RowPitch * (height_ + i), uv_plane_ + width_ * i, width_);
		}
	}
	else if (dxgi_format_ == VE_OPT_FORMAT_B8G8R8A8) {
		for (int y = 0; y < height_; y++) {
			memcpy((uint8_t*)dsec.pData + y * dsec.RowPitch,
					in_image.data() + y * width_ * 4, width_ * 4);
		}
	}
	else {
		return -3;
	}

	const NvEncInputFrame* input_frame = nv_encoder_->GetNextInputFrame();
	ID3D11Texture2D* input_texture = reinterpret_cast<ID3D11Texture2D*>(input_frame->inputPtr);
	d3d11_context_->CopyResource(input_texture, d3d11_copy_texture_);

	std::vector<std::vector<uint8_t>> packets;
	nv_encoder_->EncodeFrame(packets);

	int frame_size = 0;
	for (std::vector<uint8_t>& packet : packets) {
		out_frame.insert(out_frame.end(), packet.begin(), packet.end());
		frame_size += (int)packet.size();
	}

	return frame_size;
}

int NvidiaD3D11Encoder::Encode(HANDLE shared_handle, std::vector<uint8_t>& out_frame)
{
	if (!nv_encoder_) {
		return -1;
	}

	UpdateEvent();
	out_frame.clear();

	ID3D11Texture2D* shared_texture = nullptr;
	HRESULT hr = d3d11_device_->OpenSharedResource((HANDLE)(uintptr_t)shared_handle, 
													__uuidof(ID3D11Texture2D),
													reinterpret_cast<void**>(&shared_texture));

	if (FAILED(hr)) {
		return -2;
	}

	const NvEncInputFrame* input_frame = nv_encoder_->GetNextInputFrame();
	ID3D11Texture2D* input_texture = reinterpret_cast<ID3D11Texture2D*>(input_frame->inputPtr);
	d3d11_context_->CopyResource(input_texture, shared_texture);

	std::vector<std::vector<uint8_t>> packets;
	nv_encoder_->EncodeFrame(packets);

	if (shared_texture) {
		shared_texture->Release();
	}

	int frame_size = 0;
	for (std::vector<uint8_t>& packet : packets) {
		out_frame.insert(out_frame.end(), packet.begin(), packet.end());
		frame_size += (int)packet.size();
	}

	return frame_size;
}

bool NvidiaD3D11Encoder::InitD3D11()
{
	ClearD3D11();

	HRESULT hr = S_OK;
	DXGI_ADAPTER_DESC adapter_desc;
	D3D11_TEXTURE2D_DESC texture_desc;
	char desc[128] = { 0 };

	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&d3d11_factory_);
	if (FAILED(hr)) {
		LOG("Failed to create dxgi factory, %ld.\n", hr);
		goto failed;
	}

	for (int index = 0; index <= 1; index++) {
		hr = d3d11_factory_->EnumAdapters(index, &d3d11_adapter_);
		if (FAILED(hr)) {
			goto failed;
		}

		d3d11_adapter_->GetDesc(&adapter_desc);
		wcstombs(desc, adapter_desc.Description, sizeof(desc));
		if (strstr(desc, "NVIDIA") != NULL) {
			break;
		}
		else {
			d3d11_adapter_->Release();
			d3d11_adapter_ = nullptr;
		}
	}

	if (!d3d11_adapter_) {
		goto failed;
	}

	hr = D3D11CreateDevice(d3d11_adapter_, 
							D3D_DRIVER_TYPE_UNKNOWN, 
							NULL, 
							0, 
							NULL, 
							0, 
							D3D11_SDK_VERSION,
							&d3d11_device_,
							nullptr, 
							&d3d11_context_);
	if(FAILED(hr)) {
		printf("Failed to create d3d11 device, %ld.\n", hr);
		goto failed;
	}

	memset(&texture_desc, 0, sizeof(D3D11_TEXTURE2D_DESC));
	texture_desc.Width = width_;
	texture_desc.Height = height_;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = static_cast<DXGI_FORMAT>(dxgi_format_);
	texture_desc.SampleDesc.Count = 1;
	texture_desc.Usage = D3D11_USAGE_STAGING;
	texture_desc.BindFlags = 0;
	texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = d3d11_device_->CreateTexture2D(&texture_desc, nullptr, &d3d11_copy_texture_);
	if (FAILED(hr)) {
		LOG("Failed to create texture, %ld.\n", hr);
		return false;
	}

	return true;

failed:
	ClearD3D11();
	return false;
}

void NvidiaD3D11Encoder::ClearD3D11()
{
	if (d3d11_adapter_) {
		d3d11_adapter_->Release();
		d3d11_adapter_ = nullptr;
	}

	if (d3d11_factory_) {
		d3d11_factory_->Release();
		d3d11_factory_ = nullptr;
	}

	if (d3d11_copy_texture_) {
		d3d11_copy_texture_->Release();
		d3d11_copy_texture_ = nullptr;
	}

	if (d3d11_device_) {
		d3d11_device_->Release();
		d3d11_device_ = nullptr;
	}

	if (d3d11_context_) {
		d3d11_context_->Release();
		d3d11_context_ = nullptr;
	}
}

bool NvidiaD3D11Encoder::UpdateOption()
{
	gpu_index_    = GetOption(VE_OPT_GPU_INDEX, 0);
	width_        = GetOption(VE_OPT_WIDTH, 1920);
	height_       = GetOption(VE_OPT_HEIGHT, 1080);
	bitrate_kbps_ = GetOption(VE_OPT_BITRATE_KBPS, 8000);
	frame_rate_   = GetOption(VE_OPT_FRAME_RATE, 30);
	gop_          = GetOption(VE_OPT_GOP, 300);
	dxgi_format_  = GetOption(VE_OPT_TEXTURE_FORMAT, 87);
	codec_        = GetOption(VE_OPT_CODEC, VE_OPT_CODEC_H264);

	if (dxgi_format_ == VE_OPT_FORMAT_NV12) {
		nv_buffer_format_ = NV_ENC_BUFFER_FORMAT_NV12;
	}
	else if (dxgi_format_ == VE_OPT_FORMAT_B8G8R8A8) {
		nv_buffer_format_ = NV_ENC_BUFFER_FORMAT_ARGB;
	}
	else {
		LOG("Format unsupported.");
		return false;
	}

	if (codec_ == VE_OPT_CODEC_H264) {
		nv_codec_id_ = NV_ENC_CODEC_H264_GUID;
	}
	else if (codec_ == VE_OPT_CODEC_HEVC) {
		nv_codec_id_ = NV_ENC_CODEC_HEVC_GUID;
	}
	else {
		LOG("Codec unsupported.\n");
		return false;
	}

	return true;
}

void NvidiaD3D11Encoder::UpdateEvent()
{
	if (!nv_encoder_) {
		return ;
	}

	std::map<int, int> encoder_events = GetEvent();
	if (encoder_events.empty()) {
		return;
	}

	bool config_updated = false;
	NV_ENC_RECONFIGURE_PARAMS reconfigure_params;
	NV_ENC_CONFIG encode_config = { NV_ENC_CONFIG_VER };
	reconfigure_params.version = NV_ENC_RECONFIGURE_PARAMS_VER;
	reconfigure_params.forceIDR = true;
	reconfigure_params.reInitEncodeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
	reconfigure_params.reInitEncodeParams.encodeConfig = &encode_config;
	nv_encoder_->GetInitializeParams(&reconfigure_params.reInitEncodeParams);

	for (auto iter : encoder_events) {
		int event = iter.first;
		int value = iter.second;

		switch (event)
		{
		case VE_EVENT_FORCE_IDR:
			nv_encoder_->ForceIDR();
			break;

		case VE_EVENT_RESET_BITRATE_KBPS: 
			{
				int new_bitrate = value * 1000;
				int current_bitrate = reconfigure_params.reInitEncodeParams.encodeConfig->rcParams.averageBitRate;
				if (new_bitrate != current_bitrate) {
					config_updated = true;
					reconfigure_params.reInitEncodeParams.encodeConfig->rcParams.averageBitRate = new_bitrate;
					reconfigure_params.reInitEncodeParams.encodeConfig->rcParams.maxBitRate = new_bitrate;
				}
			}
			break;

		case VE_EVENT_RESET_FRAME_RATE:
			{
				int new_framerate = value;
				int current_framerate = reconfigure_params.reInitEncodeParams.frameRateNum;
				if (new_framerate != current_framerate) {
					config_updated = true;
					reconfigure_params.reInitEncodeParams.frameRateNum = new_framerate;
				}
			}
			break;

		default:
			break;
		}
	}

	if (config_updated) {
		nv_encoder_->Reconfigure(&reconfigure_params);
	}
}

}
