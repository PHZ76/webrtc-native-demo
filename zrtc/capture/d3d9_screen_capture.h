// PHZ
// 2021-6-25

#pragma once

#include "screen_capture.h"
#include "window_helper.h"
#include <string>

namespace DX {

class D3D9ScreenCapture : public ScreenCapture
{
public:
	D3D9ScreenCapture();
	virtual ~D3D9ScreenCapture();

	virtual bool Init(int display_index = 0);
	virtual void Destroy();

	virtual bool Capture(Image& image);

private:
	DX::Monitor monitor_;
	IDirect3D9* d3d9_ = NULL;
	IDirect3DDevice9* d3d9_device_ = NULL;
	IDirect3DSurface9* surface_ = NULL;
};

}