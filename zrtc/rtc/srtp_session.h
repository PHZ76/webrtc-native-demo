#pragma once

#include "rtc_common.h"
#include "srtp.h"

class SrtpSession
{
public:
	SrtpSession();
	virtual ~SrtpSession();

	bool Init(std::string send_key, std::string recv_key);
	void Destroy();

	int ProtectRtp(uint8_t* pkt, int len);
	int ProtectRtcp(uint8_t* pkt, int len);
	int UnprotectRtp(uint8_t* pkt, int len);
	int UnprotectRtcp(uint8_t* pkt, int len);

private:
	srtp_t send_session_ = nullptr;
	srtp_t recv_session_ = nullptr;
};

