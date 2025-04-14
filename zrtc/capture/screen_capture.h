#pragma once

#include <cstdint>
#include <vector>
#include <Windows.h>

namespace DX {

struct Image
{
	std::vector<uint8_t> bgra;
	int width;
	int height;

	HANDLE shared_handle;
};

class ScreenCapture
{
public:
	ScreenCapture& operator=(const ScreenCapture&) = delete;
	ScreenCapture(const ScreenCapture&) = delete;
	ScreenCapture() {}
	virtual ~ScreenCapture() {}

	virtual bool Init(int display_index = 0) = 0;
	virtual void Destroy() = 0;

	virtual bool Capture(Image& image) = 0;

protected:

};

}
