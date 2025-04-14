#include "dtls_connection.h"
#include "rtc_utils.h"
#include "rtc_log.h"

DtlsCert::DtlsCert()
{

}

DtlsCert::~DtlsCert()
{
	Destroy();
}

bool DtlsCert::Init()
{
	dtls_pkey_ = EVP_PKEY_new();
	if (dtls_pkey_ == nullptr) {
		return false;
	}

	EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
	if (ec_key == nullptr) {
		return false;
	}

	EC_KEY_set_asn1_flag(ec_key, OPENSSL_EC_NAMED_CURVE);
	EC_KEY_generate_key(ec_key);
	EVP_PKEY_assign_EC_KEY(dtls_pkey_, ec_key);

	dtls_cert_ = X509_new();
	if (dtls_cert_ == nullptr) {
		return false;
	}
	X509_NAME* subject = X509_NAME_new();
	if (subject == nullptr) {
		return false;
	}

	std::mt19937 mt(std::random_device{}());
	int serial = (int)mt();
	ASN1_INTEGER_set(X509_get_serialNumber(dtls_cert_), serial);

	const std::string& aor = "zrtc";
	X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, (unsigned char*)aor.data(), (int)aor.size(), -1, 0);

	X509_set_issuer_name(dtls_cert_, subject);
	X509_set_subject_name(dtls_cert_, subject);

	int expire_day = 365;
	const long cert_duration = 60 * 60 * 24 * expire_day;
	X509_gmtime_adj(X509_get_notBefore(dtls_cert_), 0);
	X509_gmtime_adj(X509_get_notAfter(dtls_cert_), cert_duration);

	X509_set_version(dtls_cert_, 2);
	X509_set_pubkey(dtls_cert_, dtls_pkey_);
	X509_sign(dtls_cert_, dtls_pkey_, EVP_sha1());

	X509_NAME_free(subject);

	uint8_t md[EVP_MAX_MD_SIZE];
	uint32_t n = 0;
	X509_digest(dtls_cert_, EVP_sha256(), md, &n);
	std::shared_ptr<char[]> fp(new char[3 * n]);
	char* p = fp.get();
	for (unsigned int i = 0; i < n; i++, ++p) {
		int nb = snprintf(p, 3, "%02X", md[i]);
		p += nb;
		if (i < (n - 1)) {
			*p = ':';
		}
		else {
			*p = '\0';
		}
	}
	fingerprint_.assign(fp.get(), strlen(fp.get()));
	return true;
}

void DtlsCert::Destroy()
{
	if (dtls_pkey_) {
		EVP_PKEY_free(dtls_pkey_);
		dtls_pkey_ = nullptr;
	}

	if (dtls_cert_) {
		X509_free(dtls_cert_);
		dtls_cert_ = nullptr;
	}
}

std::string DtlsCert::GetFingerprint()
{
	return fingerprint_;
}

X509* DtlsCert::GetCert()
{
	return dtls_cert_;
}

EVP_PKEY* DtlsCert::GetPkey()
{
	return dtls_pkey_;
}

DtlsConnection::DtlsConnection()
{

}

DtlsConnection::~DtlsConnection()
{
	Destroy();
}

bool DtlsConnection::Init(RtcRole role)
{
	role_ = role;

	if (!dtls_cert_.Init()) {
		RTC_LOG_ERROR("init dtls-cert failed.");
		return false;
	}

	if (!InitSSL()) {
		dtls_cert_.Destroy();
		return false;
	}

	return true;
}

void DtlsConnection::Destroy()
{
	is_handshake_done_ = false;

	if (bio_write_ != nullptr) {
		BIO_free(bio_write_);
		bio_write_ = nullptr;
	}

	if (bio_read_ != nullptr) {
		BIO_free(bio_read_);
		bio_read_ = nullptr;
	}

	if (ssl_ctx_ != nullptr) {
		SSL_CTX_free(ssl_ctx_);
		ssl_ctx_ = nullptr;
	}

	if (ssl_ctx_ != nullptr) {
		SSL_free(ssl_);
		ssl_ = nullptr;
	}

	dtls_cert_.Destroy();
}

std::string DtlsConnection::GetFingerprint()
{
	return dtls_cert_.GetFingerprint();
}

std::string DtlsConnection::GetSrtpSendKey()
{
	return send_key_;
}

std::string DtlsConnection::GetSrtpRecvKey()
{
	return recv_key_;
}

static int ssl_verify_callback(int ok, X509_STORE_CTX* ctx)
{
	return 1;
}

void ssl_info_callback(const SSL* ssl, int where, int ret)
{
	DtlsConnection* dtls_conn = static_cast<DtlsConnection*>(SSL_get_ex_data(ssl, 0));
	if (dtls_conn == nullptr) {
		return;
	}

	const char* method;
	int w = where & ~SSL_ST_MASK;
	if (w & SSL_ST_CONNECT) {
		method = "SSL_connect";
	}
	else if (w & SSL_ST_ACCEPT) {
		method = "SSL_accept";
	}
	else {
		method = "undefined";
	}

	if (where & SSL_CB_HANDSHAKE_DONE) {
		dtls_conn->OnHandshakeDone();
	}
}

bool DtlsConnection::InitSSL()
{
	ssl_ctx_ = SSL_CTX_new(DTLS_method());
	if (ssl_ctx_ == nullptr) {
		RTC_LOG_ERROR("SSL_CTX_new failed.");
		return false;
	}

	int ret = SSL_CTX_use_certificate(ssl_ctx_, dtls_cert_.GetCert());
	if (ret == 0) {
		RTC_LOG_ERROR("SSL_CTX_use_certificate failed:{}", ret);
		goto failed;
	}
	ret = SSL_CTX_use_PrivateKey(ssl_ctx_, dtls_cert_.GetPkey());
	if (ret == 0) {
		RTC_LOG_ERROR("SSL_CTX_use_PrivateKey failed:{}", ret);
		goto failed;
	}
	ret = SSL_CTX_check_private_key(ssl_ctx_);
	if (ret == 0) {
		RTC_LOG_ERROR("SSL_CTX_check_private_key failed:{}", ret);
		goto failed;
	}
	
	SSL_CTX_set_cipher_list(ssl_ctx_, "ALL");
	SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, ssl_verify_callback);
	SSL_CTX_set_verify_depth(ssl_ctx_, 4);
	SSL_CTX_set_read_ahead(ssl_ctx_, 1);

	ssl_ = SSL_new(ssl_ctx_);
	if (ssl_ == nullptr) {
		RTC_LOG_ERROR("SSL_new failed.");
		goto failed;
	}
	bio_read_ = BIO_new(BIO_s_mem());
	bio_write_ = BIO_new(BIO_s_mem());
	if (bio_read_ == nullptr || bio_write_ == nullptr) {
		RTC_LOG_ERROR("BIO_new failed.");
		goto failed;
	}
	SSL_set_bio(ssl_, bio_read_, bio_write_);
	SSL_set_mtu(ssl_, RTC_MAX_PACKET_SIZE);

	SSL_set_ex_data(ssl_, 0, this);
	SSL_CTX_set_info_callback(ssl_ctx_, ssl_info_callback);

	ret = SSL_CTX_set_tlsext_use_srtp(ssl_ctx_, "SRTP_AES128_CM_SHA1_80");
	if (ret != 0) {
		RTC_LOG_ERROR("SSL_CTX_set_tlsext_use_srtp failed.");
		goto failed;
	}

	return true;

failed:
	if (bio_write_ != nullptr) {
		BIO_free(bio_write_);
		bio_write_ = nullptr;
	}
	if (bio_read_ != nullptr) {
		BIO_free(bio_read_);
		bio_read_ = nullptr;
	}
	if (ssl_ctx_ != nullptr) {
		SSL_CTX_free(ssl_ctx_);
		ssl_ctx_ = nullptr;
	}

	if (ssl_ctx_ != nullptr) {
		SSL_free(ssl_);
		ssl_ = nullptr;
	}
	return false;
}

void DtlsConnection::Listen()
{
	if (ssl_ == nullptr) {
		return;
	}

	if (role_ == RTC_ROLE_SERVER) {
		SSL_set_accept_state(ssl_);
		SSL_do_handshake(ssl_);
	}
}

std::vector<uint8_t> DtlsConnection::Connect()
{
	std::vector<uint8_t> request;

	if (ssl_ == nullptr) {
		return request;
	}

	if (role_ == RTC_ROLE_CLIENT) {
		SSL_set_connect_state(ssl_);
		SSL_do_handshake(ssl_);

		if (!BIO_eof(bio_write_)) {
			char* contents{ nullptr };
			int64_t sent_bytes = BIO_get_mem_data(bio_write_, &contents);
			if (sent_bytes > 0) {
				request.assign(contents, contents + sent_bytes);
			}
			BIO_reset(bio_write_);
		}
	}

	return request;
}

bool DtlsConnection::IsHandshakeDone()
{
	return is_handshake_done_;
}

void DtlsConnection::OnHandshakeDone()
{
	RTC_LOG_INFO("ssl handshake done, role:{}", (int)role_);
	is_handshake_done_ = true;

	uint8_t out[(RTC_SRTP_KEY_LEN + RTC_SRTP_SALT_LEN) * 2] = { 0 };
	const char* label = "EXTRACTOR-dtls_srtp";
	int ret = SSL_export_keying_material(ssl_, out, sizeof(out), label, strlen(label), NULL, 0, 0);
	if (ret == 0) {
		RTC_LOG_ERROR("SSL_export_keying_material failed.");
		return;
	}

	size_t offset = 0;
	std::string client_key(reinterpret_cast<char*>(out), RTC_SRTP_KEY_LEN);
	offset += RTC_SRTP_KEY_LEN;
	std::string server_key(reinterpret_cast<char*>(out + offset), RTC_SRTP_KEY_LEN);
	offset += RTC_SRTP_KEY_LEN;
	std::string client_salt(reinterpret_cast<char*>(out + offset), RTC_SRTP_SALT_LEN);
	offset += RTC_SRTP_SALT_LEN;
	std::string server_salt(reinterpret_cast<char*>(out + offset), RTC_SRTP_SALT_LEN);

	if (role_ == RTC_ROLE_SERVER) {
		recv_key_ = client_key + client_salt;
		send_key_ = server_key + server_salt;
	}
	else if (role_ == RTC_ROLE_CLIENT) {
		recv_key_ = server_key + server_salt;
		send_key_ = client_key + client_salt;
	}
}

std::vector<uint8_t> DtlsConnection::OnRecv(const uint8_t* data, size_t size)
{
	std::vector<uint8_t> response;

	if (ssl_ == nullptr) {
		return response;
	}

	BIO_write(bio_read_, data, (int)size);
	
	// read
	uint8_t buf[1500] = { 0 };
	int read_bytes = SSL_read(ssl_, buf, sizeof(buf));
	if (read_bytes > 0) {
		RTC_LOG_INFO("SSL_read bytes:{}", read_bytes);
	}

	// send
	if (!BIO_eof(bio_write_)) {
		char* contents{ nullptr };
		int64_t bytes_send = BIO_get_mem_data(bio_write_, &contents);
		if (bytes_send > 0) {
			response.assign(contents, contents + bytes_send);
		}
		BIO_reset(bio_write_);
	}

	return response;
}

