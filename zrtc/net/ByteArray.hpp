#ifndef BYTE_ARRAY_H
#define BYTE_ARRAY_H

#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>

static uint32_t ReadU32BE(uint8_t* data, size_t size)
{
	if (size < 4) {
		return 0;
	}

	uint32_t value = 0;
	value |= (data[0] << 24);
	value |= (data[1] << 16);
	value |= (data[2] << 8);
	value |= (data[3]);
	return value;
}

inline uint16_t ReadU16BE(uint8_t* data, size_t size)
{
	if (size < 2) {
		return 0;
	}
	uint16_t value = 0;
	value |= (data[0] << 8);
	value |= (data[1]);
	return value;
}

inline void WriteUint32BE(uint8_t* data, uint32_t value) {
	data[0] = static_cast<uint8_t>(value >> 24);
	data[1] = static_cast<uint8_t>(value >> 16);
	data[2] = static_cast<uint8_t>(value >> 8);
	data[3] = static_cast<uint8_t>(value & 0xff);
}

inline void WriteUint16BE(uint8_t* data, uint32_t value) {
	data[0] = static_cast<uint8_t>(value >> 8);
	data[1] = static_cast<uint8_t>(value & 0xff);
}

class ByteArray
{
public:
	ByteArray() {
		data_.resize(0);
	}

	ByteArray(const ByteArray& ba) {
		index_ = ba.index_;
		size_ = ba.size_;
		data_ = ba.data_;
	}

	ByteArray(const uint8_t* data, size_t size) {
		index_ = 0;
		size_ = size;
		data_.resize(size_);
		if (size_ > 0) {
			memcpy(&data_[0], data, size_);
		}
	}

	ByteArray(const std::string& content) {
		index_ = 0;
		size_ = (int)content.size();
		data_.resize(size_);

		if (size_ > 0) {
			memcpy(&data_[0], content.c_str(), size_);
		}
	}

	virtual ~ByteArray() {
		index_ = 0;
		size_ = 0;
	}

	inline uint8_t* Data() {
		return &data_[0];
	}

	inline size_t Size() {
		return size_;
	}

	inline size_t Pos() {
		return index_;
	}

	inline void Write(const char* data, size_t size) {
		resize(size);
		memcpy(&data_[0] + index_, data, size);
		index_ += size;
	}

	inline size_t Read(void* data, size_t size) {
		size_t cap = size_ - index_;
		if (size > cap) {
			size = cap;
		}

		memcpy(data, &data_[0] + index_, size);
		index_ += size;
		return size;
	}

	inline size_t ReadString(std::string& data, size_t size) {
		size_t cap = size_ - index_;
		if (size > cap) {
			size = cap;
		}

		data.assign((char*)&data_[0] + index_, size);
		index_ += size;
		return size;
	}

	inline void Seek(size_t pos) {
		index_ = pos;

		if (index_ > size_) {
			index_ = size_;
		}

		if (index_ < 0) {
			index_ = 0;
		}
	}

	inline void WriteUint8(uint8_t value) {
		resize(1);
		data_[index_++] = value;
	}

	inline void WriteUint16BE(uint16_t value) {
		resize(2);
		data_[index_++] = value >> 8;
		data_[index_++] = value & 0xff;
	}

	inline void WriteUint24BE(uint32_t value) {
		resize(3);
		data_[index_++] = value >> 16;
		data_[index_++] = value >> 8;
		data_[index_++] = value & 0xff;
	}

	inline void WriteUint32BE(uint32_t value) {
		resize(4);
		data_[index_++] = value >> 24;
		data_[index_++] = value >> 16;
		data_[index_++] = value >> 8;
		data_[index_++] = value & 0xff;
	}

	inline void WriteUint16LE(uint16_t value) {
		resize(2);
		data_[index_++] = value & 0xff;
		data_[index_++] = value >> 8;
	}

	inline void WriteUint24LE(uint32_t value) {
		resize(3);
		data_[index_++] = value & 0xff;
		data_[index_++] = value >> 8;
		data_[index_++] = value >> 16;
	}

	inline void WriteUint32LE(uint32_t value) {
		resize(4);
		data_[index_++] = value & 0xff;
		data_[index_++] = value >> 8;
		data_[index_++] = value >> 16;
		data_[index_++] = value >> 24;
	}

	inline uint32_t ReadUint32BE() {
		uint32_t value = 0;
		if (size_ - index_ < 4) {
			return value;
		}

		value = (data_[index_] << 24) | (data_[index_ + 1] << 16) | (data_[index_ + 2] << 8) | data_[index_ + 3];
		index_ += 4;
		return value;
	}

	inline uint32_t ReadUint32LE() {
		uint32_t value = 0;
		if (size_ - index_ < 4) {
			return value;
		}

		value = (data_[index_ + 3] << 24) | (data_[index_ + 2] << 16) | (data_[index_ + 1] << 8) | data_[index_];
		index_ += 4;
		return value;
	}

	inline uint32_t ReadUint24BE() {
		uint32_t value = 0;
		if (size_ - index_ < 3) {
			return value;
		}

		value = (data_[index_] << 16) | (data_[index_ + 1] << 8) | data_[index_ + 2];
		index_ += 3;
		return value;
	}

	inline uint32_t ReadUint24LE() {
		uint32_t value = 0;
		if (size_ - index_ < 3) {
			return value;
		}

		value = (data_[index_ + 2] << 16) | (data_[index_ + 1] << 8) | data_[index_];
		index_ += 3;
		return value;
	}

	inline uint16_t ReadUint16BE() {
		uint32_t value = 0;
		if (size_ - index_ < 2) {
			return value;
		}

		value = (data_[index_] << 8) | data_[index_ + 1];
		index_ += 2;
		return value;
	}

	inline uint16_t ReadUint16LE() {
		uint32_t value = 0;
		if (size_ - index_ < 2) {
			return value;
		}

		value = (data_[index_ + 1] << 8) | data_[index_];
		index_ += 2;
		return value;
	}

private:
	inline void resize(size_t new_size) {
		if (size_ - index_ < new_size) {
			data_.resize(size_ + new_size);
			size_ += new_size;
		}
	}

	size_t index_ = 0;
	size_t size_ = 0;
	std::vector<uint8_t> data_;
};

#endif