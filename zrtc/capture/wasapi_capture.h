// https://docs.microsoft.com/en-us/previous-versions//ms678709(v=vs.85)

#ifndef WASAPI_CAPTURE_H
#define WASAPI_CAPTURE_H

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <wrl.h>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <vector>
#include <memory>
#include <thread>

class WASAPICapture
{
public:
	typedef std::function<void(const WAVEFORMATEX *mixFormat, uint8_t *data, uint32_t samples)> FrameCallback;

	WASAPICapture();
	~WASAPICapture();
	WASAPICapture &operator=(const WASAPICapture &) = delete;
	WASAPICapture(const WASAPICapture &) = delete;

	bool Init();
	void Destroy();

	void SetFrameCallback(const FrameCallback& callback);
	bool StartCapture();
	void StopCapture();

	WAVEFORMATEX* GetAudioFormat() const;

private:
	int  AdjustFormatTo16Bits(WAVEFORMATEX *pwfx);
	bool Capture();
	
	bool is_init_ = false;
	bool is_start_ = false;
	std::shared_ptr<std::thread> capture_thread_;

	FrameCallback frame_callback_;

	WAVEFORMATEX* mix_format_ = nullptr;
	REFERENCE_TIME actual_duration_ = 0;
	uint32_t buffer_frame_count_ = 0;
	Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
	Microsoft::WRL::ComPtr<IMMDevice> device_;
	Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
	Microsoft::WRL::ComPtr<IAudioCaptureClient> audio_capture_client_;

	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	const IID IID_IAudioClient = __uuidof(IAudioClient);
	const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
	const int REFTIMES_PER_SEC = 10000000;
	const int REFTIMES_PER_MILLISEC = 10000;

};

#endif