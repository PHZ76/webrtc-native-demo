#pragma once

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <wrl.h>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

class WASAPIPlayer
{
public:
	typedef std::function<void(const WAVEFORMATEX *mixFormat, uint8_t *data, uint32_t samples)> FrameCallback;

	WASAPIPlayer();
	~WASAPIPlayer();
	WASAPIPlayer &operator=(const WASAPIPlayer &) = delete;
	WASAPIPlayer(const WASAPIPlayer &) = delete;

	bool Init();
	void Destroy();

	void SetFrameCallback(const FrameCallback& callback);
	bool StartPlay();
	void StopPlay();

private:
	int  AdjustFormatTo16Bits(WAVEFORMATEX *pwfx);
	bool Play();

	bool is_init_ = false;
	bool is_start_ = false;
	std::shared_ptr<std::thread> play_thread_;
	FrameCallback frame_callback_;

	WAVEFORMATEX* mix_format_ = nullptr;
	REFERENCE_TIME actual_duration_ = 0;
	uint32_t buffer_frame_count_ = 0;
	Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
	Microsoft::WRL::ComPtr<IMMDevice> device_;
	Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
	Microsoft::WRL::ComPtr<IAudioRenderClient> audio_render_client_;

	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	const IID IID_IAudioClient = __uuidof(IAudioClient);
	const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
	const int REFTIMES_PER_SEC = 10000000;
	const int REFTIMES_PER_MILLISEC = 10000;
};

