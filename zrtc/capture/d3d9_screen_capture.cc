#include "d3d9_screen_capture.h"

using namespace DX;

#define DX_SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } } 

D3D9ScreenCapture::D3D9ScreenCapture()
{

}

D3D9ScreenCapture::~D3D9ScreenCapture()
{
	Destroy();
}

bool D3D9ScreenCapture::Init(int display_index)
{
	std::vector<DX::Monitor> monitors = DX::GetMonitors();
	if (monitors.size() < ((size_t)display_index + 1)) {
		return false;
	}

	monitor_ = monitors[display_index];

	HRESULT hr = S_OK;
	D3DPRESENT_PARAMETERS present_params;

	int width = monitor_.right - monitor_.left;
	int height = monitor_.bottom - monitor_.top;
	UINT adapter = display_index;

	d3d9_ = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d9_) {
		printf("[D3D9ScreenCapture] Direct3DCreate9() failed. \n");
		goto failed;
	}

	memset(&present_params, 0, sizeof(D3DPRESENT_PARAMETERS));
	present_params.Windowed = true;
	present_params.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	present_params.hDeviceWindow = GetDesktopWindow();
	hr = d3d9_->CreateDevice(
		adapter,
		D3DDEVTYPE_HAL, 
		0,
		D3DCREATE_HARDWARE_VERTEXPROCESSING, 
		&present_params, 
		&d3d9_device_
	);
	if (FAILED(hr)) {
		printf("[D3D9ScreenCapture] IDirect3D9::CreateDevice() failed, %x\n", hr);
		goto failed;
	}

	hr = d3d9_device_->CreateOffscreenPlainSurface(
		width, 
		height,
		D3DFMT_A8R8G8B8, 
		D3DPOOL_SCRATCH,
		&surface_, 
		NULL
	);
	if (FAILED(hr)) {
		printf("[D3D9ScreenCapture] IDirect3DDevice9::CreateOffscreenPlainSurface() failed, %x\n", hr);
		goto failed;
	}

	return true;

failed:
	DX_SAFE_RELEASE(surface_);
	DX_SAFE_RELEASE(d3d9_device_);
	DX_SAFE_RELEASE(d3d9_);
	return false;
}

void D3D9ScreenCapture::Destroy()
{
	DX_SAFE_RELEASE(surface_);
	DX_SAFE_RELEASE(d3d9_device_);
	DX_SAFE_RELEASE(d3d9_);
}

bool D3D9ScreenCapture::Capture(Image& image)
{
	if (!surface_) {
		return false;
	}

	int width  = monitor_.right - monitor_.left;
	int height = monitor_.bottom - monitor_.top;

	int image_size = width * height * 4;
	if (image.bgra.size() != image_size) {
		image.bgra.resize(image_size);
	}

	HRESULT hr = d3d9_device_->GetFrontBufferData(0, surface_);
	if (FAILED(hr)) {
		return false;
	}

	D3DLOCKED_RECT rect;
	ZeroMemory(&rect, sizeof(rect));
	if (surface_->LockRect(&rect, 0, 0) != S_OK) {
		return true;
	}

	image.width = width;
	image.height = height;
	image.bgra.assign((uint8_t*)rect.pBits, (uint8_t*)rect.pBits + image_size);

	surface_->UnlockRect();

	return true;
}
