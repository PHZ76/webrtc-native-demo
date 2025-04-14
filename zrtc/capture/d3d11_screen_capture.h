// PHZ
// 2020-11-20

#pragma once

#include "screen_capture.h"
#include "window_helper.h"
#include <cstdint>
#include <string>
#include <mutex>
#include <thread>
#include <memory>
#include <wrl.h>
#include <dxgi.h>
#include <d3d11_1.h>

namespace DX {

class D3D11ScreenCapture : public ScreenCapture
{
public:
	D3D11ScreenCapture();
	virtual ~D3D11ScreenCapture();

	virtual bool Init(int display_index = 0);
	virtual void Destroy();

	virtual bool Capture(Image& image);

	// save bmp faile
	bool SaveToFile(std::string pathname);

private:
	bool InitD3D11();
	void CleanupD3D11();
	bool CreateTexture();
	int  AcquireFrame();
	void CaptureFrame();

	DX::Monitor monitor_;

	int  display_index_ = 0;
	bool is_started_ = false;
	std::unique_ptr<std::thread> capture_thread_;

	std::mutex mutex_;
	std::shared_ptr<uint8_t> image_;
	uint32_t image_size_;

	// d3d resource
	DXGI_OUTDUPL_DESC dxgi_desc_;
	HANDLE shared_handle_;
	Microsoft::WRL::ComPtr<ID3D11Device>           d3d11_device_;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>    d3d11_context_;
	Microsoft::WRL::ComPtr<IDXGIOutputDuplication> dxgi_output_duplication_;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>        shared_texture_;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>        rgba_texture_;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>        gdi_texture_;
};

}
