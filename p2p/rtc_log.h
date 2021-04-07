#pragma once

#include "rtc_base/logging.h"
#include "rtc_base/log_sinks.h"

class RTCLogger
{
public:
	static RTCLogger& Instance() {
		static RTCLogger rtc_logger;
		return rtc_logger;
	}

	RTCLogger()
		: buffer_(new char[kMaxBufferSize])
	{
		//rtc::LogMessage::LogToDebug(rtc::LS_ERROR);
		rtc::LogMessage::SetLogToStderr(true);
		rtc::LogMessage::LogThreads();
	}

	~RTCLogger() {

	}

	bool Init(std::string log_dir, std::string log_prefix)
	{
		if (log_sink_) {
			return false;
		}

		log_sink_.reset(new rtc::FileRotatingLogSink(log_dir, log_prefix, kMaxLogSize, 10));
		if (!log_sink_->Init()) {
			RTC_LOG(LS_ERROR) << "Failed to open log file.";
			log_sink_.reset();
			return false;
		}
		
		rtc::LogMessage::AddLogToStream(log_sink_.get(), rtc::LS_INFO);
		return true;
	}

	void Destroy()
	{
		if (log_sink_) {
			rtc::LogMessage::RemoveLogToStream(log_sink_.get());
			log_sink_.reset();
		}
	}

	void Log(rtc::LoggingSeverity severity, const char* __func, int __line, const char* fmt, ...)
	{
		memset(buffer_.get(), 0, kMaxBufferSize);
		sprintf(buffer_.get(), "[%s:%d] ", __func, __line);
		va_list args;
		va_start(args, fmt);
		vsprintf(buffer_.get() + strlen(buffer_.get()), fmt, args);
		va_end(args);

		RTC_LOG_FILE_LINE(severity, NULL, NULL) << buffer_;
	}

private:
	std::unique_ptr<rtc::FileRotatingLogSink> log_sink_;
	std::unique_ptr<char[]> buffer_;
	static const int kMaxLogSize    = 1024 * 1024 * 10;
	static const int kMaxBufferSize = 1024 * 1024;
};

#define LOG_INFO(fmt, ...)  RTCLogger::Instance().Log(rtc::LS_INFO,  __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) RTCLogger::Instance().Log(rtc::LS_ERROR, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
