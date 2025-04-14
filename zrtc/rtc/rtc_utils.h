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
