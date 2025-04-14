#ifndef AUIDO_BUFFER_H
#define AUIDO_BUFFER_H

#include <cstdint>
#include <vector>
#include <string>
#include <memory>  
#include <mutex>

class AudioBuffer
{
public:
	AudioBuffer(uint32_t size = 10240) 
		: buffer_size_(size)
	{
		buffer_.resize(size);
	}

	~AudioBuffer()
	{

	}

	int Write(const char *data, uint32_t size)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		uint32_t bytes = WritableBytes();

		if (bytes < size) {			
			size = bytes;
		}

		if (size > 0) {
			memcpy(BeginWrite(), data, size);
			writer_index_ += size;
		}

		Retrieve(0);
		return size;
	}

	int Read(char *data, uint32_t size)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (size > ReadableBytes()) {		
			Retrieve(0);
			return -1;
		}

		memcpy(data, Peek(), size);
		Retrieve(size);
		return size;
	}

	uint32_t Size()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return ReadableBytes();
	}

	void Clear()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		RetrieveAll();
	}

private:

	uint32_t ReadableBytes() const
	{
		return static_cast<uint32_t>(writer_index_ - reader_index_);
	}

	uint32_t WritableBytes() const
	{
		return static_cast<uint32_t>(buffer_.size() - writer_index_);
	}

	char* Peek()
	{
		return Begin() + reader_index_;
	}

	const char* Peek() const
	{
		return Begin() + reader_index_;
	}

	void RetrieveAll()
	{
		writer_index_ = 0;
		reader_index_ = 0;
	}

	void Retrieve(size_t len)
	{
		if (len > 0 && len <= ReadableBytes()) {
			reader_index_ += len;
			if (reader_index_ == writer_index_) {
				reader_index_ = 0;
				writer_index_ = 0;
			}
		}

		if (reader_index_ > 0 && writer_index_ > 0) {
			buffer_.erase(buffer_.begin(), buffer_.begin() + reader_index_);
			buffer_.resize(buffer_size_);
			writer_index_ -= reader_index_;
			reader_index_ = 0;		
		}
	}

	void RetrieveUntil(const char* end)
	{
		Retrieve(end - Peek());
	}

	char* Begin()
	{
		return &*buffer_.begin();
	}

	const char* Begin() const
	{
		return &*buffer_.begin();
	}

	char* BeginWrite()
	{
		return Begin() + writer_index_;
	}

	const char* BeginWrite() const
	{
		return Begin() + writer_index_;
	}

	std::mutex mutex_;
	std::vector<char> buffer_;
	uint32_t buffer_size_ = 0;
	size_t reader_index_ = 0;
	size_t writer_index_ = 0;
};

#endif