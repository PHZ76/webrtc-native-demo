#pragma once

#include "stun.h"
#include <unordered_map>
#include <string>

class StunReceiver
{
public:
	StunReceiver();
	virtual ~StunReceiver();

	bool Parse(const uint8_t* data, size_t size);

	std::string GetTransactionId() const;
	std::string GetAttrUserName() const;
	std::string GetMessageIntegrity() const;

private:
	uint16_t message_type_ = 0;
	uint32_t message_length_ = 0;
	uint32_t mapped_addr_ = 0;
	uint32_t mapped_port_ = 0;
	uint32_t magic_cookie_ = 0;
	uint32_t priority_ = 0;
	std::string transaction_id_;
	std::string username_;
	std::string password_;
	std::string message_integrity_;
};

