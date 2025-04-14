#pragma once

#include <cstdint> 
#include <utility> 

class H264Parser
{
public:
    static std::pair<uint8_t*, uint8_t*> find_nalu(const uint8_t *data, uint32_t size);
  
};

