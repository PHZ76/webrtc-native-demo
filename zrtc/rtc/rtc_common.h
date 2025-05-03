#pragma once

#include "rtc_utils.h"
#include "rtc_log.h"
#include "rtp.h"
#include "rtcp.h"
#include "stun.h"
#include "srtp.h"

static const uint32_t MAX_MTU = 1500;
static const uint32_t RTC_MAX_PACKET_SIZE= 1350;

static const uint32_t RTC_ICE_UFRAG_LENGTH = 4;
static const uint32_t RTC_ICE_PASSWORD_LENGTH = 24;

static const uint32_t RTC_SRTP_KEY_LEN = 16;
static const uint32_t RTC_SRTP_SALT_LEN = 14;

static const uint32_t RTC_MAX_RTP_PACKET_LENGTH = RTC_MAX_PACKET_SIZE - SRTP_MAX_TRAILER_LEN;
static const uint32_t RTC_MAX_RTCP_PACKET_LENGTH = RTC_MAX_PACKET_SIZE - SRTP_MAX_TRAILER_LEN;

static const uint8_t  RTC_OPUS_PAYLOAD_TYPE = 111;

static const uint8_t   RTC_H264_PAYLOAD_TYPE = 125;
static const uint32_t  RTC_H264_CLOCK_RATE = 90000;
static const uint8_t   RTC_H264_FRAME_TYPE_IDR = 5;
static const uint8_t   RTC_H264_FRAME_TYPE_SPS = 7;
static const uint8_t   RTC_H264_FRAME_TYPE_PPS = 8;
static const uint8_t   RTC_H264_FRAME_TYPE_REF = 1;

static const uint32_t  RTC_RTCP_UPDATE_INTERVAL = 1000;

enum RtcMediaCodec
{
	RTC_MEDIA_CODEC_H264 = 102,
	RTC_MEDIA_CODEC_OPUS = 111,

	RTC_MEDIA_CODEC_RTX  = 120,
};

enum RtcRole
{
	RTC_ROLE_UNDEFINE = 0,
	RTC_ROLE_CLIENT = 1,
	RTC_ROLE_SERVER = 2,
};
