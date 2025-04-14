#pragma once

#include "rtc_common.h"
#include "openssl/x509.h"
#include "openssl/ssl.h"
#include "openssl/bio.h"
#include <string>
#include <cstdint>
#include <vector>

static bool IsDtlsPacket(const uint8_t* data, size_t len)
{
	return ((len >= 13) && (data[0] > 19 && data[0] < 64));
}

class DtlsCert
{
public:
	DtlsCert();
	virtual ~DtlsCert();

	bool Init();
	void Destroy();

	std::string GetFingerprint();
	X509* GetCert();
	EVP_PKEY* GetPkey();

private:
	EVP_PKEY* dtls_pkey_ = nullptr;
	X509* dtls_cert_ = nullptr; // 证书
	std::string fingerprint_;
};

class DtlsConnection
{
public:
	DtlsConnection();
	virtual ~DtlsConnection();

	bool Init(RtcRole role);
	void Destroy();

	std::string GetFingerprint();
	std::string GetSrtpSendKey();
	std::string GetSrtpRecvKey();

	void Listen();
	std::vector<uint8_t> Connect();
	std::vector<uint8_t> OnRecv(const uint8_t* data, size_t size);

	bool IsHandshakeDone();
	void OnHandshakeDone();

private:
	bool InitSSL();

	DtlsCert dtls_cert_;
	RtcRole role_ = RTC_ROLE_UNDEFINE;

	SSL_CTX* ssl_ctx_ = nullptr;
	SSL* ssl_ = nullptr;
	BIO* bio_read_ = nullptr;
	BIO* bio_write_ = nullptr;

	bool is_handshake_done_ = false;
	std::string send_key_;
	std::string recv_key_;
};
