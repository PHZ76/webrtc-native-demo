#include "rtc_sdp.h"
#include "rtc_utils.h"

#include <iostream>
#include <sstream>

static const std::string key_ice_ufrag = "a=ice-ufrag:";
static const std::string key_ice_pwd = "a=ice-pwd:";
static const std::string key_fingerprint = "a=fingerprint:";
static const std::string key_rtpmap = "a=rtpmap:";

static RTPMap ParseRTPMapLine(const std::string& line)
{
    RTPMap rtp_map;
    std::istringstream iss(line);
    std::string token;

    if (std::getline(iss, token, ':')) { // Skip 'a=rtpmap:'
        if (std::getline(iss, token, ' ')) {
            rtp_map.payload_type = std::stoi(token);
            if (std::getline(iss, token, '/')) {
                rtp_map.encoding_name = token;
                if (std::getline(iss, token)) {
                    rtp_map.clock_rate = std::stoi(token);
                }
            }
        }
    }
    return rtp_map;
}

RtcSdp::RtcSdp()
{

}

RtcSdp::~RtcSdp()
{

}

bool RtcSdp::Parse(std::string sdp)
{
    auto tokens = SplitString(sdp, '\n');
    for (auto token : tokens) {
        if (token.find(key_ice_ufrag) != std::string::npos) {
            ice_ufrag_ = token.substr(key_ice_ufrag.size());
        }
        else if (token.find(key_ice_pwd) != std::string::npos) {
            ice_pwd_ = token.substr(key_ice_pwd.size());
        }
        else if (token.find(key_fingerprint) != std::string::npos) {
            fingerprint_ = token.substr(key_fingerprint.size());
        }
        else if (token.find(key_rtpmap) != std::string::npos) {
            rtp_maps_.push_back(ParseRTPMapLine(token));
        }
    }

    if (ice_ufrag_.empty()) {
        return false;
    }

    return true;
}

std::string RtcSdp::Build()
{
    std::ostringstream ss;
    ss << "v=0\n";
    ss << "o=- 1369034262975887107 2 IN IP4 0.0.0.0\n";
    ss << "s=-\n";
    ss << "t=0 0\n";
    ss << "a=msid-semantic: WMS\n";
    ss << "a=ice-lite\n";
    ss << "a=group:BUNDLE 0 1\n";

    ss << "m=audio 9 UDP/TLS/RTP/SAVPF " << audio_payload_type_ << "\n";
    ss << "a=rtpmap:" << audio_payload_type_ << " opus/48000/2\n";
    //ss << "a=fmtp:" << audio_payload_type_ << " minptime=10;stereo=1;useinbandfec=1\n";
    ss << "c=IN IP4 0.0.0.0\n";
    ss << "a=ice-ufrag:" << ice_ufrag_ << "\n";
    ss << "a=ice-pwd:" << ice_pwd_ << "\n";
    ss << "a=fingerprint:sha-256 " << fingerprint_ << "\n";
    ss << "a=setup:passive\n";
    ss << "a=mid:0\n";
    ss << "a=sendonly\n";
    ss << "a=rtcp-mux\n";
    ss << "a=rtcp-rsize\n";
    //a=extmap:
    ss << "a=rtcp-fb:" << audio_payload_type_ << " transport-cc\n";
    ss << "a=ssrc:" << audio_ssrc_ << " cname:" << stream_name_ << "\n";
    ss << "a=ssrc:" << audio_ssrc_ << " label:" << stream_name_ << "_audio\n";
    ss << "a=candidate:0 1 UDP 2130706431 " << ip_ << " " << port_ << " typ host generation 0\n";

    ss << "m=video 9 UDP/TLS/RTP/SAVPF " << video_payload_type_ << "\n";
    ss << "a=rtpmap:" << video_payload_type_ << " H264/90000\n";
    ss << "c=IN IP4 0.0.0.0\n";
    ss << "a=ice-ufrag:" << ice_ufrag_ << "\n";
    ss << "a=ice-pwd:" << ice_pwd_ << "\n";
    ss << "a=fingerprint:sha-256 " << fingerprint_ << "\n";
    ss << "a=setup:passive\n";
    ss << "a=mid:1\n";
    ss << "a=sendonly\n";
    ss << "a=rtcp-mux\n";
    ss << "a=rtcp-rsize\n";
    //a=extmap:
    //ss << "a=rtcp-fb:" << video_payload_type_ << " ccm fir\n";
    //ss << "a=rtcp-fb:" << video_payload_type_ << " goog-remb\n";
    ss << "a=rtcp-fb:" << video_payload_type_ << " transport-cc\n";
    ss << "a=rtcp-fb:" << video_payload_type_ << " nack\n";
    ss << "a=rtcp-fb:" << video_payload_type_ << " nack pli\n";
    ss << "a=fmtp:" << video_payload_type_ << " level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\n";
    //ss << "a=fmtp:" << video_payload_type_ << " packetization-mode=1;profile-level-id=42e01f\r\n";
    ss << "a=ssrc:" << video_ssrc_ << " cname:" << stream_name_ << "\n";
    ss << "a=ssrc:" << video_ssrc_ << " label:" << stream_name_ << "_video\n";
    ss << "a=candidate:0 1 UDP 2130706431 " << ip_ << " " << port_ << " typ host generation 0\n";

    return ss.str();
}

void RtcSdp::SetAddress(std::string ip, uint16_t port)
{
    ip_ = ip;
    port_ = port;
}

void RtcSdp::SetIceParams(std::string ice_ufrag, std::string ice_pwd)
{
    ice_ufrag_ = ice_ufrag;
    ice_pwd_ = ice_pwd;
}

void RtcSdp::SetFingerprint(std::string fingerprint)
{
    fingerprint_ = fingerprint;
}

void RtcSdp::SetStreamName(std::string stream_name)
{
    stream_name_ = stream_name;
}

void RtcSdp::SetVideoSsrc(uint32_t ssrc)
{
    video_ssrc_ = ssrc;
}

void RtcSdp::SetAudioSsrc(uint32_t ssrc)
{
    audio_ssrc_ = ssrc;
}

void RtcSdp::SetVideoPayloadType(uint32_t payload_type)
{
    video_payload_type_ = payload_type;
}

void RtcSdp::SetAudioPayloadType(uint32_t payload_type)
{
    audio_payload_type_ = payload_type;
}

std::string RtcSdp::GetIceUfrag()
{
    return ice_ufrag_;
}