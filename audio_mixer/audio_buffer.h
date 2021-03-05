#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>  
#include <mutex>

class AudioBuffer
{
public:
	AudioBuffer(uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_sample, uint32_t max_samples)
		: sample_rate_ (sample_rate)
		, channels_(channels)
		, bytes_per_sample_(bytes_per_sample)
		, max_samples_(max_samples)
	{
		max_buffer_size_ = max_samples * channels * bytes_per_sample;
		buffer_.resize(max_buffer_size_);
	}

	virtual ~AudioBuffer() 
	{
		Clear();
	}

	int Write(void* data, uint32_t samples) 
	{
		std::lock_guard<std::mutex> lock(mutex_);

		uint32_t bytes_write = samples * channels_ * bytes_per_sample_;

		if (WritableBytes() < bytes_write) {
			RetrieveAll();
			return -1;
		}

		memcpy(BeginWrite(), data, bytes_write);
		writer_index_ += bytes_write;
		samples_ += samples;

		return samples;
	}

	int Read(void* data, uint32_t samples)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		uint32_t bytes_read = samples * channels_ * bytes_per_sample_;

		if (ReadableBytes() < bytes_read) {
			return -1;
		}

		memcpy(data, Peek(), bytes_read);
		samples_ -= samples;

		Retrieve(bytes_read);

		return samples;
	}

	void Clear()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		RetrieveAll();
	}

	int GetSamples() 
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return samples_;
	}

private:
	uint32_t ReadableBytes() const
	{
		return writer_index_ - reader_index_;
	}

	uint32_t WritableBytes() const 
	{
		return (static_cast<uint32_t>(buffer_.size()) - writer_index_);
	}

	void RetrieveAll()
	{
		reader_index_ = 0;
		writer_index_ = 0;
		samples_ = 0;
	}

	void Retrieve(uint32_t size) 
	{
		if (size <= ReadableBytes()) {
			reader_index_ += size;
			if (reader_index_ == writer_index_) {
				reader_index_ = 0;
				writer_index_ = 0;
			}
		}

		if (reader_index_ > (max_buffer_size_ / 2) && writer_index_ > 0) {
			buffer_.erase(buffer_.begin(), buffer_.begin() + reader_index_);
			buffer_.resize(max_buffer_size_);
			writer_index_ -= reader_index_;
			reader_index_ = 0;
		}
	}

	uint8_t* Peek() 
	{
		return Begin() + reader_index_;
	}

	uint8_t* Begin() 
	{
		return &*buffer_.begin();
	}

	uint8_t* BeginWrite() 
	{
		return Begin() + writer_index_;
	}

	std::mutex mutex_;
	std::vector<uint8_t> buffer_;
	uint32_t reader_index_     = 0;
	uint32_t writer_index_     = 0;
	uint32_t samples_          = 0;
	uint32_t sample_rate_      = 0;
	uint32_t channels_         = 0;
	uint32_t bytes_per_sample_ = 0;
	uint32_t max_samples_      = 0;
	uint32_t max_buffer_size_  = 0;
};
