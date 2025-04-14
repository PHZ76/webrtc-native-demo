#include "wasapi_player.h"

WASAPIPlayer::WASAPIPlayer()
{

}

WASAPIPlayer::~WASAPIPlayer()
{

}

bool WASAPIPlayer::Init()
{
	if (is_init_) {
		return false;
	}

	CoInitialize(NULL);

	HRESULT hr = S_OK;
	hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)enumerator_.GetAddressOf());
	if (FAILED(hr)) {
		printf("[WASAPIPlayer] Failed to create instance.\n");
		return false;
	}

	hr = enumerator_->GetDefaultAudioEndpoint(eRender, eMultimedia, device_.GetAddressOf());
	if (FAILED(hr)) {
		printf("[WASAPIPlayer] Failed to create device.\n");
		return false;
	}

	hr = device_->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)audio_client_.GetAddressOf());
	if (FAILED(hr)) {
		printf("[WASAPIPlayer] Failed to activate device.\n");
		return false;
	}

	hr = audio_client_->GetMixFormat(&mix_format_);
	if (FAILED(hr)) {
		printf("[WASAPIPlayer] Failed to get mix format.\n");
		return false;
	}

	AdjustFormatTo16Bits(mix_format_);
	actual_duration_ = REFTIMES_PER_SEC;
	hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, actual_duration_, 0, mix_format_, NULL);
	if (FAILED(hr)) {
		printf("[WASAPIPlayer] Failed to initialize audio client.\n");
		return false;
	}

	hr = audio_client_->GetBufferSize(&buffer_frame_count_);
	if (FAILED(hr)) {
		printf("[WASAPIPlayer] Failed to get buffer size.\n");
		return false;
	}

	hr = audio_client_->GetService(IID_IAudioRenderClient, (void**)audio_render_client_.GetAddressOf());
	if (FAILED(hr))
	{
		printf("[WASAPIPlayer] Failed to get service.\n");
		return false;
	}

	// Calculate the actual duration of the allocated buffer.
	actual_duration_ = REFTIMES_PER_SEC * buffer_frame_count_ / mix_format_->nSamplesPerSec;

	is_init_ = true;
	return true;
}

void WASAPIPlayer::Destroy()
{
	if (is_start_) {
		StopPlay();
	}

	if (is_init_) {
		enumerator_.Reset();
		device_.Reset();
		audio_client_.Reset();
		audio_render_client_.Reset();
		CoUninitialize();
		is_init_ = false;
	}
}

void WASAPIPlayer::SetFrameCallback(const FrameCallback& callback)
{
	frame_callback_ = callback;
}

int WASAPIPlayer::AdjustFormatTo16Bits(WAVEFORMATEX *pwfx)
{
	if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
		pwfx->wFormatTag = WAVE_FORMAT_PCM;
	}
	else if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
		if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) {
			pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			pEx->Samples.wValidBitsPerSample = 16;
		}
	}
	else {
		return -1;
	}

	pwfx->wBitsPerSample = 16;
	pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
	pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
	return 0;
}

bool WASAPIPlayer::StartPlay()
{
	if (!is_init_) {
		return false;
	}

	HRESULT hr = audio_client_->Start();
	if (FAILED(hr)) {
		printf("[WASAPIPlayer] Failed to start audio client.\n");
		return false;
	}

	is_start_ = true;
	play_thread_.reset(new std::thread([this] {
		while (is_start_) {
			if (!Play()) {
				break;
			}
		}
	}));

	return true;
}

void WASAPIPlayer::StopPlay()
{
	if (!is_start_) {
		return;
	}

	is_start_ = false;
	if (play_thread_) {
		play_thread_->join();
		play_thread_.reset();
	}

	HRESULT hr = audio_client_->Stop();
	if (FAILED(hr)) {
		printf("[WASAPIPlayer] Failed to stop audio client.\n");
	}
}

bool WASAPIPlayer::Play()
{
	uint32_t num_frames_padding = 0;
	uint32_t num_frames_available = 0;

	HRESULT hr = audio_client_->GetCurrentPadding(&num_frames_padding);
	if (FAILED(hr)) {
		printf("[WASAPIPlayer] Failed to get current padding.\n");
		return false;
	}

	num_frames_available = buffer_frame_count_ - num_frames_padding;

	if (num_frames_available > 0) {
		BYTE *data;
		hr = audio_render_client_->GetBuffer(num_frames_available, &data);
		if (FAILED(hr)) {
			printf("[WASAPIPlayer] Audio render client failed to get buffer.\n");
			return false;
		}

		if (frame_callback_) {
			memset(data, 0, mix_format_->nBlockAlign * num_frames_available);
			frame_callback_(mix_format_, data, num_frames_available);
		}

		hr = audio_render_client_->ReleaseBuffer(num_frames_available, 0);
		if (FAILED(hr)) {
			printf("[WASAPIPlayer] Audio render client failed to release buffer.\n");
			return false;
		}
	}
	else {
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}

	return true;
}