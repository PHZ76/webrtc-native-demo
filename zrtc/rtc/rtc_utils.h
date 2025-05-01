#pragma once

#include "net/EventLoop.h"
#include "net/NetInterface.h"
#include "net/Timer.h"
#include "net/Timestamp.h"
#include "net/SocketUtil.h"
#include "net/ByteArray.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <random>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/time.h>
#include <ctime>
#endif

static std::vector<std::string> SplitString(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    for (char ch : str) {
        if (ch == delimiter) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        }
        else {
            token += ch;
        }
    }

    if (!token.empty()) {
        tokens.push_back(token);
    }
    return tokens;
}

static uint32_t GenerateSSRC()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, INT_MAX);
    return static_cast<uint32_t>(dis(gen));
}

static std::string GenerateRandomString(size_t length)
{
    const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, (int)charset.size() - 1); // 均匀分布

    std::string random_string;
    random_string.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        random_string += charset[dis(gen)];
    }

    // std::transform(random_string.begin(), random_string.end(), random_string.begin(), ::tolower);

    return random_string;
}

static uint64_t GetSysTimestamp()
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

static uint64_t GetNtpTimestamp()
{
    uint64_t ntp_time = 0;

#if defined(_WIN32) || defined(_WIN64)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    const uint64_t epoch_diff = 116444736000000000ULL;
    uint64_t unix_time_100ns = uli.QuadPart - epoch_diff;
    uint64_t seconds = unix_time_100ns / 10000000ULL;
    uint64_t fraction = (unix_time_100ns % 10000000ULL) * 4294967296ULL / 10000000ULL;
    ntp_time = (seconds + 2208988800ULL) << 32;
    ntp_time |= fraction;
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    uint64_t seconds = tv.tv_sec + 2208988800ULL;
    uint64_t fraction = tv.tv_usec * 4294967296ULL / 1000000ULL;
    ntp_time = (seconds << 32) | fraction;
#endif
    return ntp_time;
}