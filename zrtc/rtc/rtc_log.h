#ifndef XOP_RTC_LOG_H
#define XOP_RTC_LOG_H

#include <iostream>
#include <memory>

// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_sinks.h"

#define RTC_LOG_TAG "rtc_log"

#define RTC_LOG_INFO(msg, ...) \
{ \
    if (spdlog::get(RTC_LOG_TAG))  \
        {spdlog::get(RTC_LOG_TAG)->info(msg, ##__VA_ARGS__);} \
    else \
        {spdlog::info(msg, ##__VA_ARGS__);} \
}

#define RTC_LOG_ERROR(msg, ...) \
{ \
    if (spdlog::get(RTC_LOG_TAG))  \
        {spdlog::get(RTC_LOG_TAG)->error(msg, ##__VA_ARGS__);} \
    else \
        {spdlog::error(msg, ##__VA_ARGS__);} \
}


static void init_rtc_log(std::string pathname = "rtc_log/log.txt")
{
    try {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(pathname.c_str(), true);
        std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks = { console_sink, file_sink };
        auto logger = std::make_shared<spdlog::logger>(RTC_LOG_TAG, begin(sinks), end(sinks));
        spdlog::details::registry::instance().register_logger(logger);
    }
    catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "init log failed: " << ex.what() << std::endl;
    }
}

#endif