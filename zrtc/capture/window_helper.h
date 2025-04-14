#pragma once
#include <vector>
#include <d3d9.h>

namespace DX {

struct Monitor
{
	uint64_t low_part;
	uint64_t high_part;

	int left;
	int top;
	int right;
	int bottom;
};

std::vector<Monitor> GetMonitors();

}
