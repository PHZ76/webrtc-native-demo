#include "wasapi_capture.h"

#pragma comment(lib, "ole32.lib")

WASAPICapture::WASAPICapture()
{

}

WASAPICapture::~WASAPICapture()
{
	Destroy();
}


bool WASAPICapture::Init()
{
	if (is_init_) {
		return false;
	}

	CoInitialize(NULL);
	//CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	HRESULT hr = S_OK;
	hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)enumerator_.GetAddressOf());
	if (FAILED(hr))  {
		printf("[WASAPICapture] Failed to create instance.\n");
		return false;
	}

	//hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, device_.GetAddressOf());
	hr = enumerator_->GetDefaultAudioEndpoint(eCapture, eConsole, device_.GetAddressOf());
	if (FAILED(hr)) {
		printf("[WASAPICapture] Failed to create device.\n");
		return false;
	}

	hr = device_->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)audio_client_.GetAddressOf());
	if (FAILED(hr)) {
		printf("[WASAPICapture] Failed to activate device.\n");
		return false;
	}

	hr = audio_client_->GetMixFormat(&mix_format_);
	if (FAILED(hr)) {
		printf("[WASAPICapture] Failed to get mix format.\n");
		return false;
	}

	AdjustFormatTo16Bits(mix_format_);
	actual_duration_ = REFTIMES_PER_SEC;
	//hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, actual_duration_, 0, mix_format_, NULL);
	hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, actual_duration_, 0, mix_format_, NULL);
	if (FAILED(hr)) {
		printf("[WASAPICapture] Failed to initialize audio client.\n");
		return false;
	}

	hr = audio_client_->GetBufferSize(&buffer_frame_count_);
	if (FAILED(hr)) {
		printf("[WASAPICapture] Failed to get buffer size.\n");
		//return false;
	}

	hr = audio_client_->GetService(IID_IAudioCaptureClient, (void**)audio_capture_client_.GetAddressOf());
	if (FAILED(hr)) {
		printf("[WASAPICapture] Failed to get service.\n");
		return false;
	}

	// Calculate the actual duration of the allocated buffer.
	actual_duration_ = REFERENCE_TIME(REFTIMES_PER_SEC * buffer_frame_count_ / mix_format_->nSamplesPerSec);

	is_init_ = true;
	return true;
}

void WASAPICapture::Destroy()
{
	if (is_start_) {
		StopCapture();
	}

	if (is_init_) {
		enumerator_.Reset();
		device_.Reset();
		audio_client_.Reset();
		audio_capture_client_.Reset();
		CoUninitialize();
		is_init_ = false;
	}
}

void WASAPICapture::SetFrameCallback(const FrameCallback& callback)
{
	frame_callback_ = callback;
}

WAVEFORMATEX* WASAPICapture::GetAudioFormat() const
{
	return mix_format_;
}

int WASAPICapture::AdjustFormatTo16Bits(WAVEFORMATEX *pwfx)
{
	if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
	{
		pwfx->wFormatTag = WAVE_FORMAT_PCM;
	}
	else if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
		if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat))
		{
			pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			pEx->Samples.wValidBitsPerSample = 16;
		}
	}
	else
	{
		return -1;
	}

	pwfx->wBitsPerSample = 16;
	pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
	pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
	return 0;
}

bool WASAPICapture::StartCapture()
{
	if (!is_init_) {
		return false;
	}

	HRESULT hr = audio_client_->Start();
	if (FAILED(hr)) {
		printf("[WASAPICapture] Failed to start audio client.\n");
		return false;
	}
	
	is_start_ = true;
	capture_thread_.reset(new std::thread([this] {
		while (is_start_) { 
			if (!Capture()) {
				break;
			}
		}
	}));

	return 0;
}

void WASAPICapture::StopCapture()
{
	if (!is_start_) {
		return;
	}

	is_start_ = false;
	if (capture_thread_) {
		capture_thread_->join();
		capture_thread_ = nullptr;
	}

	HRESULT hr = audio_client_->Stop();
	if (FAILED(hr)) {
		printf("[WASAPICapture] Failed to stop audio client.\n");
		return ;
	}
}

bool WASAPICapture::Capture()
{
	HRESULT hr = S_OK;
	uint32_t packet_length = 0;
	uint32_t num_frames_available = 0;
	BYTE *pData = NULL;
	DWORD flags = 0;

	hr = audio_capture_client_->GetNextPacketSize(&packet_length);
	if (FAILED(hr)) {
		printf("[WASAPICapture] Faild to get next data packet size.\n");
		return false;
	}

	if (packet_length == 0) {
		timeBeginPeriod(1);
		::Sleep(1);
		timeEndPeriod(1);
		return true;
	}

	while (packet_length > 0) {
		hr = audio_capture_client_->GetBuffer(&pData, &num_frames_available, &flags, NULL, NULL);
		if (FAILED(hr)) {
			printf("[WASAPICapture] Faild to get buffer.\n");
			return false;
		}

		if (frame_callback_) {
			frame_callback_(mix_format_, pData, num_frames_available);
		}

		hr = audio_capture_client_->ReleaseBuffer(num_frames_available);
		if (FAILED(hr)) {
			printf("[WASAPICapture] Faild to release buffer.\n");
			return false;
		}

		hr = audio_capture_client_->GetNextPacketSize(&packet_length);
		if (FAILED(hr)) {
			printf("[WASAPICapture] Faild to get next data packet size.\n");
			return false;
		}
	}

	return true;
}