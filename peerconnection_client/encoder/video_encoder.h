#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include <map>

namespace xop
{

enum VIDEO_ENCODER_OPTION_CODEC_VALUE
{
	VE_OPT_CODEC_H264 = 1,
	VE_OPT_CODEC_HEVC = 2
};

enum VIDEO_ENCODER_OPTION_TEXTURE_FORMAT_VALUE
{
	VE_OPT_FORMAT_B8G8R8A8 = 87,
	VE_OPT_FORMAT_NV12     = 103,
};

enum VIDEO_ENCODER_OPTION
{
	VE_OPT_UNKNOW = 0,
	VE_OPT_GPU_INDEX,
	VE_OPT_WIDTH,
	VE_OPT_HEIGHT,
	VE_OPT_FRAME_RATE,
	VE_OPT_BITRATE_KBPS,
	VE_OPT_GOP,
	VE_OPT_CODEC,
	VE_OPT_TEXTURE_FORMAT,
};

enum VIDEO_ENCODER_EVENT
{
	VE_EVENT_UNKNOW = 0,
	VE_EVENT_FORCE_IDR,
	VE_EVENT_RESET_BITRATE_KBPS,
	VE_EVENT_RESET_FRAME_RATE
};

class VideoEncoder
{
public:
	VideoEncoder& operator=(const VideoEncoder&) = delete;
	VideoEncoder(const VideoEncoder&) = delete;
	VideoEncoder() {}
	virtual ~VideoEncoder() {}

	//  set before Init()
	void SetOption(int option, int value)
	{
		std::lock_guard<std::mutex> locker(option_mutex_);
		encoder_options_[option] = value;
	}

	int GetOption(int option, int default_value)
	{
		std::lock_guard<std::mutex> locker(option_mutex_);
		auto iter = encoder_options_.find(option);
		if (iter != encoder_options_.end()) {
			return iter->second;
		}
		return default_value;
	}

	//  set before Encode()
	void SetEvent(int event, int value)
	{
		std::lock_guard<std::mutex> locker(event_mutex_);
		encoder_events_[event] = value;
	}

	std::map<int, int> GetEvent()
	{
		std::lock_guard<std::mutex> locker(event_mutex_);
		std::map<int, int> events;
		events.swap(encoder_events_);
		return events;
	}

	virtual bool Init()     = 0;
	virtual void Destroy()  = 0;

protected:
	std::mutex option_mutex_;
	std::map<int, int> encoder_options_;
	std::mutex event_mutex_;
	std::map<int, int> encoder_events_;
};

}
