#ifndef EXTERNAL_BUILTIN_VIDEO_ENCODER_FACTORY_H_
#define EXTERNAL_BUILTIN_VIDEO_ENCODER_FACTORY_H_

#include <memory>
#include "rtc_base/system/rtc_export.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {

// Creates a new factory that can create the built-in types of video decoders.
RTC_EXPORT std::unique_ptr<webrtc::VideoEncoderFactory> CreateBuiltinExternalVideoEncoderFactory();

} // namespace webrtc

#endif  // EXTERNAL_BUILTIN_VIDEO_ENCODER_FACTORY_H_