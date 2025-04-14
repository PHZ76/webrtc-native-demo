#include "srtp_session.h"

SrtpSession::SrtpSession()
{

}

SrtpSession::~SrtpSession()
{
	Destroy();
}

bool SrtpSession::Init(std::string send_key, std::string recv_key)
{
	srtp_policy_t srtp_policy;
	memset(&srtp_policy, 0, sizeof(srtp_policy_t));
	srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&srtp_policy.rtp);
	srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&srtp_policy.rtcp);
	srtp_policy.ssrc.value = 0;
	srtp_policy.window_size = 4096;
	srtp_policy.next = NULL;
	srtp_policy.allow_repeat_tx = 1;

	// send session
	srtp_policy.key = (unsigned char*)send_key.c_str();
	srtp_policy.ssrc.type = ssrc_any_outbound;
	srtp_err_status_t status = srtp_create(&send_session_ , &srtp_policy);
	if (status != srtp_err_status_ok) {
		RTC_LOG_ERROR("srtp_create failed, status:{}", (int)status);
		goto failed;
	}

	// recv session
	srtp_policy.key = (unsigned char*)recv_key.c_str();
	srtp_policy.ssrc.type = ssrc_any_inbound;
	status = srtp_create(&recv_session_, &srtp_policy);
	if (status != srtp_err_status_ok) {
		RTC_LOG_ERROR("srtp_create failed, status:{}", (int)status);
		goto failed;
	}

	return true;

failed:
	if (send_session_ != nullptr) {
		srtp_dealloc(send_session_);
		send_session_ = nullptr;
	}
	if (recv_session_ != nullptr) {
		srtp_dealloc(recv_session_);
		recv_session_ = nullptr;
	}
	return false;
}

void SrtpSession::Destroy()
{
	if (send_session_ != nullptr) {
		srtp_dealloc(send_session_);
		send_session_ = nullptr;
	}

	if (recv_session_ != nullptr) {
		srtp_dealloc(recv_session_);
		recv_session_ = nullptr;
	}
}

int SrtpSession::ProtectRtp(uint8_t* pkt, int len)
{
	if (!send_session_) {
		return 0;
	}
	srtp_err_status_t status = srtp_protect(send_session_, pkt, &len);
	if (status != srtp_err_status_ok) {
		RTC_LOG_ERROR("srtp_protect failed, status:{}", (int)status);
		return - 1;
	}
	return len;
}

int SrtpSession::ProtectRtcp(uint8_t* pkt, int len)
{
	srtp_err_status_t status = srtp_protect_rtcp(send_session_, pkt, &len);
	if (status != srtp_err_status_ok) {
		RTC_LOG_ERROR("srtp_protect_rtcp failed, status:{}", (int)status);
		return -1;
	}
	return len;
}

int SrtpSession::UnprotectRtp(uint8_t* pkt, int len)
{
	srtp_err_status_t status = srtp_unprotect(recv_session_, pkt, &len);
	if (status != srtp_err_status_ok) {
		RTC_LOG_ERROR("srtp_protect_rtcp failed, status:{}", (int)status);
		return -1;
	}
	return len;
}

int SrtpSession::UnprotectRtcp(uint8_t* pkt, int len)
{
	srtp_err_status_t status = srtp_unprotect_rtcp(recv_session_, pkt, &len);
	if (status != srtp_err_status_ok) {
		RTC_LOG_ERROR("srtp_unprotect_rtcp failed, status:{}", (int)status);
		return -1;
	}
	return len;
}