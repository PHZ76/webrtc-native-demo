#include "stun_sender.h"
#include "rtc_common.h"

#include <openssl/dh.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/ssl.h>

StunSender::StunSender()
{

}

StunSender::~StunSender()
{

}

void StunSender::SetMessageType(uint16_t message_type)
{
	message_type_ = message_type;
}

void StunSender::SetTransactionId(std::string transaction_id)
{
	transaction_id_ = transaction_id;
}

void StunSender::SetUserame(std::string username)
{
	// username = remote_ufrag + ":" + local_ufrag;
	username_ = username;
}

void StunSender::SetPassword(std::string password)
{
	password_ = password;
}

void StunSender::SetMappedAddress(uint32_t mapped_addr)
{
	mapped_addr_ = mapped_addr;
}

void StunSender::SetMappedPort(uint16_t mapped_port)
{
	mapped_port_ = mapped_port;
}

std::vector<uint8_t> StunSender::Build()
{
	buffer_.reset(new ByteArray());
	message_length_ = 0;

	buffer_->WriteUint16BE(message_type_);
	buffer_->WriteUint16BE(message_length_);
	buffer_->WriteUint32BE(STUN_MAGIC_COOKIE);
	buffer_->Write(transaction_id_.c_str(), static_cast<int>(transaction_id_.length()));

	message_length_ += EncodeUsername();
	message_length_ += EncodeMappedAddress();
	message_length_ += STUN_ATTRIBUTE_HEADER_SIZE + 20; // integrity size
	buffer_->Data()[2] = (message_length_ & 0xFF00) >> 8;
	buffer_->Data()[3] = (message_length_ & 0x00FF);
	EncodeMessageIntegrity();

	message_length_ += STUN_ATTRIBUTE_HEADER_SIZE + 4; // fingerprint size
	buffer_->Data()[2] = (message_length_ & 0xFF00) >> 8;
	buffer_->Data()[3] = (message_length_ & 0x00FF);
	EncodeFingerprint();

	return std::vector<uint8_t>(buffer_->Data(), buffer_->Data() + buffer_->Size());
}

uint16_t StunSender::EncodeUsername()
{
	if (!buffer_) {
		return 0;
	}

	// username = remote_ufrag + ":" + local_ufrag;
	buffer_->WriteUint16BE(STUN_ATTR_USERNAME);
	buffer_->WriteUint16BE(static_cast<uint16_t>(username_.size()));
	buffer_->Write(username_.c_str(), username_.size());

	char padding[4] = { 0 };
	int padding_len = 0;
	if (username_.size() % 4 != 0) {
		padding_len = 4 - username_.size() % 4;
		buffer_->Write(padding, padding_len);
	}

	return static_cast<uint16_t>(STUN_ATTRIBUTE_HEADER_SIZE + username_.size() + padding_len);
}

uint16_t StunSender::EncodeMappedAddress()
{
	if (!buffer_) {
		return 0;
	}

	// mapped address ipv4
	buffer_->WriteUint16BE(STUN_ATTR_XOR_MAPPED_ADDRESS);
	buffer_->WriteUint16BE(8);
	buffer_->WriteUint8(0);
	buffer_->WriteUint8(STUN_MAPPED_ADDR_IPV4);
	buffer_->WriteUint16BE(mapped_port_ ^ (STUN_MAGIC_COOKIE >> 16));
	buffer_->WriteUint32BE(mapped_addr_ ^ STUN_MAGIC_COOKIE);

	return STUN_ATTRIBUTE_HEADER_SIZE + 8;
}

uint16_t StunSender::EncodeMessageIntegrity()
{
	if (!buffer_) {
		return 0;
	}

	char hmac[20] = { 0 };
	uint32_t hmac_len = 0;

	const EVP_MD* evp_md = EVP_sha1();
	if (evp_md == nullptr) {
		return 0;
	}

	HMAC_CTX* hmac_ctx = HMAC_CTX_new();
	if (hmac_ctx == nullptr) {
		goto failed;
	}

	if (HMAC_Init_ex(hmac_ctx, password_.c_str(), (int)password_.length(), evp_md, NULL) < 0) {
		goto failed;
	}

	if (HMAC_Update(hmac_ctx, (const unsigned char*)buffer_->Data(), buffer_->Size()) < 0) {
		goto failed;
	}

	if (HMAC_Final(hmac_ctx, (unsigned char*)hmac, &hmac_len) < 0) {
		goto failed;
	}

	HMAC_CTX_free(hmac_ctx);

	// integrity
	buffer_->WriteUint16BE(STUN_ATTR_MESSAGE_INTEGRITY);
	buffer_->WriteUint16BE(20);
	buffer_->Write(hmac, sizeof(hmac));

	return STUN_ATTRIBUTE_HEADER_SIZE + 20;

failed:
	if (hmac_ctx) {
		RTC_LOG_ERROR("Calculate hmac failed.");
		HMAC_CTX_free(hmac_ctx);
	}
	return 0;
}

uint16_t StunSender::EncodeFingerprint()
{
	if (!buffer_) {
		return 0;
	}

	// fingerprint
	uint32_t c = 0xFFFFFFFF;
	uint8_t* data = buffer_->Data();
	size_t size = buffer_->Size();
	for (size_t index = 0; index < size; ++index) {
		c = CRC32_TABLE[(c ^ data[index]) & 0xFF] ^ (c >> 8);
	}
	uint32_t fingerprint = (c ^ 0xFFFFFFFF) ^ STUN_FINGERPRINT_XOR;

	buffer_->WriteUint16BE(STUN_ATTR_FINGERPRINT);
	buffer_->WriteUint16BE(4);
	buffer_->WriteUint32BE(fingerprint);

	return STUN_ATTRIBUTE_HEADER_SIZE + 4;
}