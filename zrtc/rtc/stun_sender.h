#pragma once

#include "stun.h"
#include "rtc_utils.h"

class StunSender
{
public:
	StunSender();
	virtual ~StunSender();

	void SetMessageType(uint16_t message_type);
	void SetTransactionId(std::string transaction_id);
	void SetUserame(std::string username);
	void SetPassword(std::string password);
	void SetMappedAddress(uint32_t mapped_addr);
	void SetMappedPort(uint16_t mapped_port);

	std::vector<uint8_t> Build();

private:
	uint16_t EncodeUsername();
	uint16_t EncodeMappedAddress();
	uint16_t EncodeMessageIntegrity();
	uint16_t EncodeFingerprint();

	std::unique_ptr<ByteArray> buffer_;
	uint16_t message_type_ = 0;
	uint16_t message_length_ = 0;
	uint32_t mapped_addr_ = 0;
	uint32_t mapped_port_ = 0;
	uint32_t magic_cookie_ = 0;
	uint32_t priority_ = 0;
	std::string transaction_id_;
	std::string username_;
	std::string password_;
	std::string message_integrity_;
};
