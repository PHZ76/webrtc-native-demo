#include "h264_parser.h"
#include <cstring>


std::pair<uint8_t*, uint8_t*> H264Parser::find_nalu(const uint8_t *data, uint32_t size)
{
    std::pair<uint8_t*, uint8_t*> nalu(nullptr, nullptr);

    if(size < 5 || data == nullptr) {
        return nalu;
    }

    nalu.second = const_cast<uint8_t*>(data) + (size-1);

    uint32_t start_code = 0;
    uint32_t pos = 0;
    uint8_t  prefix[3] = { 0, 0, 0};
    size -= 3;
    data += 2;

    while(size--) {
        if ((prefix[pos % 3] == 0) && (prefix[(pos + 1) % 3] == 0) && (prefix[(pos + 2) % 3] == 1)) {
            if(nalu.first == nullptr) {
                nalu.first = const_cast<uint8_t*>(data) - 2;
                start_code = 3; // 00 00 01
            }
            else if(start_code == 3) {
                nalu.second = const_cast<uint8_t*>(data) - 3;
                break;
            }               
        }
        else if ((prefix[pos % 3] == 0) && (prefix[(pos + 1) % 3] == 0) && (prefix[(pos + 2) % 3] == 0)) {
            if (*(data + 1) == 0x01) {              
                if(nalu.first == nullptr) {
                    if(size >= 1) {
                        nalu.first = const_cast<uint8_t*>(data) - 2;
                    }                       
                    else {
                        break;  
                    }                  
                    start_code = 4; // 00 00 00 01 
                }
                else if(start_code == 4) {
                    nalu.second = const_cast<uint8_t*>(data) - 3;
                    break;
                }                    
            }
        }

        prefix[(pos++) % 3] = *(++data);
    }
        
    if (nalu.first == nullptr) {
        nalu.second = nullptr;
    }
 
    return nalu;
}


