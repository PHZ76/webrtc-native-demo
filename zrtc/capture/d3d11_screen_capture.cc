#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "d3d11_screen_capture.h"
#include <fstream> 

using namespace DX;

D3D11ScreenCapture::D3D11ScreenCapture()
{
	memset(&monitor_, 0, sizeof(DX::Monitor));
	memset(&dxgi_desc_, 0, sizeof(dxgi_desc_));
}

D3D11ScreenCapture::~D3D11ScreenCapture()
{
	Destroy();
}

bool D3D11ScreenCapture::Init(int display_index)
{
	if (is_started_) {
		return true;
	}

	display_index_ = display_index;

	if (!InitD3D11()) {
		goto failed;
	}

	is_started_ = true;
	AcquireFrame();
	capture_thread_.reset(new std::thread([this] {
		while (is_started_) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			AcquireFrame();
		}
	}));

	return true;

failed:
	Destroy();
	return false;
}

bool D3D11ScreenCapture::InitD3D11()
{
	std::vector<DX::Monitor> monitors = DX::GetMonitors();
	if (monitors.size() < ((size_t)display_index_ + 1)) {
		return false;
	}

	monitor_ = monitors[display_index_];

	HRESULT hr = S_OK;
	D3D_FEATURE_LEVEL feature_level;
	int index = 0;
	Microsoft::WRL::ComPtr<IDXGIFactory> dxgi_factory;
	Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
	Microsoft::WRL::ComPtr<IDXGIOutput>  dxgi_output;
	Microsoft::WRL::ComPtr<IDXGIOutput1> dxgi_output1;

	hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
		d3d11_device_.GetAddressOf(), &feature_level, d3d11_context_.GetAddressOf());
	if (FAILED(hr)) {
		printf("[D3D11ScreenCapture] Failed to create d3d11 device.\n");
		return false;
	}

	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)dxgi_factory.GetAddressOf());
	if (FAILED(hr)) {
		printf("[D3D11ScreenCapture] Failed to create dxgi factory.\n");
		return false;
	}

	do
	{
		if (dxgi_factory->EnumAdapters(index, dxgi_adapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND) {
			if (dxgi_adapter->EnumOutputs(display_index_, dxgi_output.GetAddressOf()) != DXGI_ERROR_NOT_FOUND) {
				if (dxgi_output.Get() != nullptr) {
					break;
				}
			}
		}
	} while (0);

	if (dxgi_adapter.Get() == nullptr) {
		printf("[D3D11ScreenCapture] DXGI adapter not found.\n");
		return false;
	}

	if (dxgi_output.Get() == nullptr) {
		printf("[D3D11ScreenCapture] DXGI output not found.\n");
		return false;
	}


	hr = dxgi_output.Get()->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(dxgi_output1.GetAddressOf()));
	if (FAILED(hr)) {
		printf("[D3D11ScreenCapture] Failed to query interface dxgiOutput1.\n");
		return false;
	}

	hr = dxgi_output1->DuplicateOutput(d3d11_device_.Get(), dxgi_output_duplication_.GetAddressOf());
	if (FAILED(hr)) {
		/* 0x887a0004: NVIDIA控制面板-->全局设置--首选图形处理器(自动选择) */
		printf("[D3D11ScreenCapture] Failed to get duplicate output.\n");
		return false;
	}

	dxgi_output_duplication_->GetDesc(&dxgi_desc_);

	if (!CreateTexture()) {
		return false;
	}

	return true;
}

void D3D11ScreenCapture::CleanupD3D11()
{
	rgba_texture_.Reset();
	gdi_texture_.Reset();
	shared_texture_.Reset();
	dxgi_output_duplication_.Reset();
	d3d11_device_.Reset();
	d3d11_context_.Reset();
	shared_handle_ = nullptr;
	memset(&dxgi_desc_, 0, sizeof(dxgi_desc_));
}

bool D3D11ScreenCapture::CreateTexture()
{
	D3D11_TEXTURE2D_DESC desc = { 0 };
	desc.Width = dxgi_desc_.ModeDesc.Width;
	desc.Height = dxgi_desc_.ModeDesc.Height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	HRESULT hr = d3d11_device_->CreateTexture2D(&desc, nullptr, shared_texture_.GetAddressOf());
	if (FAILED(hr)) {
		printf("[D3D11ScreenCapture] Failed to create texture.\n");
		return false;
	}

	desc.BindFlags = 0;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = 0;

	hr = d3d11_device_->CreateTexture2D(&desc, nullptr, rgba_texture_.GetAddressOf());
	if (FAILED(hr)) {
		printf("[D3D11ScreenCapture] Failed to create texture.\n");
		return false;
	}

	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags = 0;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET;
	desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;

	hr = d3d11_device_->CreateTexture2D(&desc, nullptr, gdi_texture_.GetAddressOf());
	if (FAILED(hr)) {
		printf("[D3D11ScreenCapture] Failed to create texture.\n");
		return false;
	}

	Microsoft::WRL::ComPtr<IDXGIResource> dxgi_resource;
	hr = shared_texture_->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void**>(dxgi_resource.GetAddressOf()));
	if (FAILED(hr)) {
		printf("[D3D11ScreenCapture] Failed to query IDXGIResource interface from texture.\n");
		return false;
	}

	hr = dxgi_resource->GetSharedHandle(&shared_handle_);
	if (FAILED(hr)) {
		printf("[D3D11ScreenCapture] Failed to get shared handle.\n");
		return false;
	}

	return true;
}

void D3D11ScreenCapture::Destroy()
{
	is_started_ = false;
	if (capture_thread_) {
		capture_thread_->join();
		capture_thread_.reset();
	}

	CleanupD3D11();
}

int D3D11ScreenCapture::AcquireFrame()
{
	Microsoft::WRL::ComPtr<IDXGIResource> dxgi_resource;
	DXGI_OUTDUPL_FRAME_INFO frame_info;
	memset(&frame_info, 0, sizeof(DXGI_OUTDUPL_FRAME_INFO));

	dxgi_output_duplication_->ReleaseFrame();
	HRESULT hr = dxgi_output_duplication_->AcquireNextFrame(0, &frame_info, dxgi_resource.GetAddressOf());

	if (FAILED(hr)) {
		if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
			return -1;
		}
		else if (hr == DXGI_ERROR_INVALID_CALL
			|| hr == DXGI_ERROR_ACCESS_LOST) {
			CleanupD3D11();
			InitD3D11();
			return -2;
		}
		return -3;
	}

	if (frame_info.AccumulatedFrames == 0 ||
		frame_info.LastPresentTime.QuadPart == 0) {
		// No image update, only cursor moved.
	}

	if (!dxgi_resource.Get()) {
		return -1;
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D> output_texture;
	hr = dxgi_resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(output_texture.GetAddressOf()));
	if (FAILED(hr)) {
		return -1;
	}

	d3d11_context_->CopyResource(gdi_texture_.Get(), output_texture.Get());

	Microsoft::WRL::ComPtr<IDXGISurface1> surface1;
	hr = gdi_texture_->QueryInterface(__uuidof(IDXGISurface1), reinterpret_cast<void**>(surface1.GetAddressOf()));
	if (FAILED(hr)) {
		return -1;
	}

	CURSORINFO cursor_info = { 0 };
	cursor_info.cbSize = sizeof(CURSORINFO);
	if (GetCursorInfo(&cursor_info) == TRUE) {
		if (cursor_info.flags == CURSOR_SHOWING) {
			auto cursor_position = cursor_info.ptScreenPos;
			auto cursor_size = cursor_info.cbSize;
			HDC  hdc;
			surface1->GetDC(FALSE, &hdc);
			DrawIconEx(hdc, cursor_position.x - monitor_.left, cursor_position.y - monitor_.top,
				cursor_info.hCursor, 0, 0, 0, 0, DI_NORMAL | DI_DEFAULTSIZE);
			surface1->ReleaseDC(nullptr);
		}
	}

	d3d11_context_->CopyResource(rgba_texture_.Get(), gdi_texture_.Get());

	CaptureFrame();
	return 0;
}

void D3D11ScreenCapture::CaptureFrame()
{
	std::lock_guard<std::mutex> locker(mutex_);

	D3D11_MAPPED_SUBRESOURCE dsec = { 0 };

	HRESULT hr = d3d11_context_->Map(rgba_texture_.Get(), 0, D3D11_MAP_READ, 0, &dsec);
	if (!FAILED(hr)) {
		if (dsec.pData != NULL) {
			int image_width = (int)dxgi_desc_.ModeDesc.Width;
			int image_height = (int)dxgi_desc_.ModeDesc.Height;
			image_size_ = image_width * image_height * 4;
			image_.reset(new uint8_t[image_size_], std::default_delete<uint8_t[]>());

			for (int y = 0; y < image_height; y++) {
				memcpy(image_.get() + y * image_width * 4, (uint8_t*)dsec.pData + y * dsec.RowPitch, image_width * 4);
			}
		}
		d3d11_context_->Unmap(rgba_texture_.Get(), 0);
	}

	d3d11_context_->CopyResource(shared_texture_.Get(), gdi_texture_.Get());
}

bool D3D11ScreenCapture::Capture(Image& image)
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (!is_started_) {
		return false;
	}

	if (image_size_ == 0) {
		return false;
	}

	if (image.bgra.size() != image_size_) {
		image.bgra.resize(image_size_);
	}

	image.bgra.assign(image_.get(), image_.get() + image_size_);
	image.width = dxgi_desc_.ModeDesc.Width;
	image.height = dxgi_desc_.ModeDesc.Height;

	if (shared_handle_) {
		image.shared_handle = shared_handle_;
	}

	return true;
}

bool D3D11ScreenCapture::SaveToFile(std::string pathname)
{
	Image image;
	if (!Capture(image)) {
		return false;
	}

	std::ofstream fp_out(pathname.c_str(), std::ios::out | std::ios::binary);
	if (!fp_out) {
		printf("[D3D11ScreenCapture] capture image failed, open %s failed.\n", pathname.c_str());
		return false;
	}

	unsigned char file_header[54] = {
		0x42, 0x4d, 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,   /*file header*/
		40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 32, 0,  /*info header*/
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0
	};

	uint32_t image_width = image.width;
	uint32_t image_height = image.height;
	uint32_t image_size = image_width * image_height * 4;
	uint32_t file_size = sizeof(file_header) + image_size;

	file_header[2] = (uint8_t)file_size;
	file_header[3] = file_size >> 8;
	file_header[4] = file_size >> 16;
	file_header[5] = file_size >> 24;

	file_header[18] = (uint8_t)image_width;
	file_header[19] = image_width >> 8;
	file_header[20] = image_width >> 16;
	file_header[21] = image_width >> 24;

	file_header[22] = (uint8_t)image_height;
	file_header[23] = image_height >> 8;
	file_header[24] = image_height >> 16;
	file_header[25] = image_height >> 24;

	fp_out.write((char*)file_header, 54);

	char* image_data = (char*)(&image.bgra[0]);
	for (int h = image_height - 1; h >= 0; h--) {
		fp_out.write(image_data + h * image_width * 4, image_width * 4);
	}

	fp_out.close();
	return true;
}