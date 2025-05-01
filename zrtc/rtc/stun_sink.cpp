#include "stun_sink.h"
#include "rtc_common.h"

StunSink::StunSink()
{

}

StunSink::~StunSink()
{

}

bool StunSink::Parse(const uint8_t* data, size_t size)
{
	if (!IsStunPacket(data, size)) {
		return false;
	}

	ByteArray buffer(data, size);
	message_type_ = buffer.ReadUint16BE();
	message_length_ = buffer.ReadUint16BE();

	magic_cookie_ = buffer.ReadUint32BE();
	if (magic_cookie_ != STUN_MAGIC_COOKIE) {
		RTC_LOG_ERROR("Parse stun magic cookie error, magic_cookie:{}", magic_cookie_);
		return false;
	}

	buffer.ReadString(transaction_id_, STUN_TRANSACTION_ID_SIZE);

	while (buffer.Pos() < buffer.Size()) {
		uint16_t stun_attr_type = buffer.ReadUint16BE();
		uint16_t stun_attr_length = buffer.ReadUint16BE();

		switch (stun_attr_type)
		{
		case STUN_ATTR_USERNAME:
			buffer.ReadString(username_, stun_attr_length);
			break;
		case STUN_ATTR_PASSWORD:
			buffer.ReadString(password_, stun_attr_length);
			break;
		case STUN_ATTR_MESSAGE_INTEGRITY:
			buffer.ReadString(message_integrity_, stun_attr_length);
			break;
		case STUN_ATTR_PRIORITY:
			priority_ = buffer.ReadUint32BE();
			break;
		default:
			buffer.Seek(buffer.Pos() + stun_attr_length);
			break;
		}

		char padding[4] = { 0 };
		if (stun_attr_length % 4 != 0) {
			int  padding_len = 4 - stun_attr_length % 4;
			buffer.Read(padding, padding_len);
		}
	}

	return true;
}

std::string StunSink::GetTransactionId() const
{
	return transaction_id_;
}

std::string StunSink::GetAttrUserName() const
{
	return username_;
}

std::string StunSink::GetMessageIntegrity() const
{
	return message_integrity_;
}
