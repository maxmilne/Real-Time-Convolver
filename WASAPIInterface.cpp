//-----------------------------------------------------------
// Locates the Oculus Rift speaker and microphone interfaces
// and uses the WASAPI (Windows Audio Session API) to 
// create a low-latency event-driven loop. Accesses microphone
// capture data, processes it, and submits it to speaker render 
// buffer. This example convolves the microphone data with 
// a BRIR file.
//
// Adapted from Microsoft's code at
// https://msdn.microsoft.com/en-us/library/windows/desktop/dd370844(v=vs.85).aspx
//-----------------------------------------------------------

#include <Audioclient.h>
#include <Audiopolicy.h>
#include <initguid.h>
#include <Mmdeviceapi.h>
#include <Avrt.h>
#include <stdio.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <cstring>
#include <Dwmapi.h>
#include <cmath>
#include "PCMFunctions.h"
#include "ConvolverPrime.h"
#include "LoadBRIR.h"
#include <thread>
#include <iostream>
#include "PortAudioWrapper.h"

#pragma comment(lib, "Avrt.lib")
#pragma comment(lib, "Dwmapi.lib")

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
const IID IID_IAudioClock = __uuidof(IAudioClock);

static LARGE_INTEGER StartingTimeC, EndingTimeC, ElapsedMillisecondsC;
static LARGE_INTEGER FrequencyC;
static LARGE_INTEGER StartingTimeR, EndingTimeR, ElapsedMillisecondsR;
static LARGE_INTEGER FrequencyR;
static LARGE_INTEGER EndingTime;
static LARGE_INTEGER Frequency;

HRESULT convolveVoice()
{
	printf("This is a demo for the real time convolution system.\n"
		"This system was optimized for low latency with the Oculus Rift VR headset via the WASAPI.\n"
		"If you have an Oculus Rift installed, press 's' and enter to begin the convolution demo.\n"
		"Otherwise, press 'o' and enter to begin the convolution demo with this computer's\n"
		"default speaker and microphone. Please use headphones to prevent feedback.\n\n"
		"NOTE: If you are using your computer's default speaker and microphone,\n"
		"the latency will be higher, as will the potential for audio glitches.\n\n");
	//// Keyboard prep.
	DWORD        mode;
	INPUT_RECORD event;

	char key;
	while (true) {
		std::cin >> key;
		if (key == 's')
			break;
		else if (key == 'o')
			PortAudioWrapper::start();
	}

	HRESULT hr;
	REFERENCE_TIME hnsActualDuration;
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDeviceCollection *pDevicesR = NULL;
	IMMDeviceCollection *pDevicesC = NULL;
	IMMDevice *pDeviceR = NULL;
	IMMDevice *pDeviceC = NULL;
	IAudioClient *pAudioClientR = NULL;
	IAudioClient *pAudioClientC = NULL;
	IAudioRenderClient *pRenderClient = NULL;
	IAudioCaptureClient *pCaptureClient = NULL;
	IAudioClock *pRenderClock = NULL;
	IAudioClock *pCaptureClock = NULL;
	WAVEFORMATEXTENSIBLE *pwfxR = new WAVEFORMATEXTENSIBLE();
	WAVEFORMATEXTENSIBLE *pwfxR2 = NULL;
	WAVEFORMATEXTENSIBLE *pwfxC = new WAVEFORMATEXTENSIBLE();
	WAVEFORMATEXTENSIBLE *pwfxC2 = NULL;
	HANDLE lpHandles[3];
	HANDLE hTask = NULL;
	UINT32 bufferFrameCountR;
	UINT32 bufferFrameCountC;
	UINT32 numFramesAvailable;
	UINT32 numFramesPadding;
	UINT32 numFramesToRead;
	UINT32 pcDevicesR;
	UINT32 pcDevicesC;
	LPWSTR pwszID = NULL;
	REFERENCE_TIME phnsMinimumDevicePeriodR;
	REFERENCE_TIME phnsMinimumDevicePeriodC;
	IPropertyStore *pProps = NULL;
	BYTE *pDataR = NULL;
	BYTE *pDataC = NULL;
	BYTE *pData = NULL;
	DWORD flags = 0;
	DWORD pdwFlags = 0;
	BOOL foundRender = FALSE;
	BOOL foundCapture = FALSE;

	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);
	EXIT_ON_ERROR(hr)

		hr = pEnumerator->EnumAudioEndpoints(
			eRender, DEVICE_STATE_ACTIVE, &pDevicesR);
	EXIT_ON_ERROR(hr)
		hr = pEnumerator->EnumAudioEndpoints(
			eCapture, DEVICE_STATE_ACTIVE, &pDevicesC);
	EXIT_ON_ERROR(hr)

		hr = pDevicesR->GetCount(
			&pcDevicesR);
	EXIT_ON_ERROR(hr)
		hr = pDevicesC->GetCount(
			&pcDevicesC);
	EXIT_ON_ERROR(hr)

		int pcDevicesMax = max(pcDevicesR, pcDevicesC);
	IMMDevice **pDevice = NULL;
	IMMDeviceCollection *pDevices = NULL;
	WAVEFORMATEXTENSIBLE *pwfx = NULL;
	WAVEFORMATEXTENSIBLE *pwfx2 = NULL;
	BOOL *pFound = NULL;
	wchar_t* targetDevice = NULL;
	printf("Iterating through audio devices.\n");
	for (UINT32 i = 0; i < 2 * pcDevicesMax; i++) {
		if (foundRender && foundCapture)
			break;
		if (i % 2 == 0) {
			if (i / 2 > pcDevicesR || foundRender)
				continue;
			pDevice = &pDeviceR;
			pDevices = pDevicesR;
			pFound = &foundRender;
			pwfx = pwfxR;
			pwfx2 = pwfxR2;
			targetDevice = L"Headphones (Rift Audio)";
		}
		else {
			if (i / 2 > pcDevicesC || foundCapture)
				continue;
			pDevice = &pDeviceC;
			pDevices = pDevicesC;
			pFound = &foundCapture;
			pwfx = pwfxC;
			pwfx2 = pwfxC2;
			targetDevice = L"Microphone (Rift Audio)";
		}

		hr = pDevices->Item(i / 2, pDevice);
		EXIT_ON_ERROR(hr)

			// Get the endpoint ID string.
			hr = (*pDevice)->GetId(&pwszID);
		EXIT_ON_ERROR(hr)

			hr = (*pDevice)->OpenPropertyStore(
				STGM_READ, &pProps);
		EXIT_ON_ERROR(hr)

			PROPVARIANT varName;
		// Initialize container for property value.
		PropVariantInit(&varName);

		// Get the endpoint's friendly-name property.
		hr = pProps->GetValue(
			PKEY_Device_FriendlyName, &varName);
		EXIT_ON_ERROR(hr)

			//// Print endpoint friendly name and endpoint ID.
			//printf("Endpoint %d: \"%S\" (%S)\n",
			//	i / 2, varName.pwszVal, pwszID);

			*pFound = !wcscmp(varName.pwszVal, targetDevice);

		if (*pFound) {
			WAVEFORMATEXTENSIBLE *pwfxTarget = NULL;
			if (i % 2 == 0)
				pwfxTarget = pwfxR;
			else
				pwfxTarget = pwfxC;
			hr = pProps->GetValue(PKEY_AudioEngine_DeviceFormat, &varName);
			pwfx2 = (PWAVEFORMATEXTENSIBLE)varName.blob.pBlobData;
			pwfxTarget->Format = pwfx2->Format;
			pwfxTarget->dwChannelMask = pwfx2->dwChannelMask;
			pwfxTarget->Samples = pwfx2->Samples;
			pwfxTarget->SubFormat = pwfx2->SubFormat;
		}

		CoTaskMemFree(pwszID);
		pwszID = NULL;
		PropVariantClear(&varName);
		SAFE_RELEASE(pProps)
	}

	printf("Oculus Rift found.\n");

	hr = pDeviceR->Activate(
		IID_IAudioClient, CLSCTX_ALL,
		NULL, (void**)&pAudioClientR);
	EXIT_ON_ERROR(hr)
		hr = pDeviceC->Activate(
			IID_IAudioClient, CLSCTX_ALL,
			NULL, (void**)&pAudioClientC);
	EXIT_ON_ERROR(hr)

		hr = pAudioClientR->GetDevicePeriod(
			NULL, &phnsMinimumDevicePeriodR);
	EXIT_ON_ERROR(hr)
		hr = pAudioClientC->GetDevicePeriod(
			NULL, &phnsMinimumDevicePeriodC);
	EXIT_ON_ERROR(hr)

		pwfxC->Format.nSamplesPerSec = 48000;
	pwfxC->Format.nAvgBytesPerSec = 96000;
	hr = pAudioClientR->IsFormatSupported(
		AUDCLNT_SHAREMODE_EXCLUSIVE,
		(WAVEFORMATEX*)pwfxR,
		NULL);
	EXIT_ON_ERROR(hr)
		hr = pAudioClientC->IsFormatSupported(
			AUDCLNT_SHAREMODE_EXCLUSIVE,
			(WAVEFORMATEX*)pwfxC,
			NULL);
	EXIT_ON_ERROR(hr)

		hr = pAudioClientR->Initialize(
			AUDCLNT_SHAREMODE_EXCLUSIVE,
			AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			53333,
			53333,
			(WAVEFORMATEX*)pwfxR,
			NULL);
	EXIT_ON_ERROR(hr)
		hr = pAudioClientC->Initialize(
			AUDCLNT_SHAREMODE_EXCLUSIVE,
			AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			53333,
			53333,
			(WAVEFORMATEX*)pwfxC,
			NULL);
	EXIT_ON_ERROR(hr)

		// Create an event handle and register it for
		// buffer-event notifications.
		lpHandles[0] = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (lpHandles[0] == NULL)
	{
		hr = E_FAIL;
		goto Exit;
	}
	lpHandles[1] = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (lpHandles[1] == NULL)
	{
		hr = E_FAIL;
		goto Exit;
	}

	hr = pAudioClientC->SetEventHandle(lpHandles[0]);
	EXIT_ON_ERROR(hr);
	hr = pAudioClientR->SetEventHandle(lpHandles[1]);
	EXIT_ON_ERROR(hr);

	// Get the actual size of the two allocated buffers.
	hr = pAudioClientR->GetBufferSize(&bufferFrameCountR);
	EXIT_ON_ERROR(hr)
	hr = pAudioClientC->GetBufferSize(&bufferFrameCountC);
	EXIT_ON_ERROR(hr)

	PCMFunctions::init(bufferFrameCountR);

	hr = pAudioClientR->GetService(
		IID_IAudioRenderClient,
		(void**)&pRenderClient);
	EXIT_ON_ERROR(hr)
		hr = pAudioClientC->GetService(
			IID_IAudioCaptureClient,
			(void**)&pCaptureClient);
	EXIT_ON_ERROR(hr)
		hr = pAudioClientR->GetService(
			IID_IAudioClock,
			(void**)&pRenderClock);
	EXIT_ON_ERROR(hr)
		hr = pAudioClientC->GetService(
			IID_IAudioClock,
			(void**)&pCaptureClock);
	EXIT_ON_ERROR(hr)

	// Ask MMCSS to temporarily boost the thread priority
	// to reduce glitches while the low-latency stream plays.
	DWORD taskIndex = 0;
	hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
	if (hTask == NULL)
	{
		hr = E_FAIL;
		EXIT_ON_ERROR(hr)
	}

	UINT64 i1 = 0;
	UINT64 i2 = 0;
	UINT64 iLast = 0;
	UINT64 diff = 0;

	UINT64 u1 = 0;
	UINT64 u2 = 0;
	UINT64 u3 = 0;
	UINT64 u4 = 0;

	int frames = 0;
	bool started = false;

	pData = new BYTE[2 * bufferFrameCountC];
	for (int i = 0; i < 2 * bufferFrameCountC; i++) {
		pData[i] = 0;
	}

	lpHandles[2] = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(lpHandles[2], &mode);
	SetConsoleMode(lpHandles[2], 0);
	////

	// Grab the next empty buffer from the audio device.
	hr = pRenderClient->GetBuffer(bufferFrameCountR, &pDataR);
	EXIT_ON_ERROR(hr)
	//printf("Render event was signaled. frame_playing:%i qpc_time:%llu since_last_wake:%i\n", u1, u2, ElapsedMillisecondsR.QuadPart);
	//printf("Render startup fill. 0\n");
	// Preload the buffer with data from the audio source.
	memcpy(pDataR, pData, 4 * bufferFrameCountR * sizeof(BYTE));
	hr = pRenderClient->ReleaseBuffer(bufferFrameCountR, flags);
	EXIT_ON_ERROR(hr)

	printf("Starting. Press 'q' to quit.\n");
	hr = pAudioClientC->Start();  // Start playing.
	EXIT_ON_ERROR(hr)

	// Each loop fills one of the two buffers.
	double* buffL;
	double* buffR;
	double* inputBuffer;
	BYTE* tempL;
	BYTE* tempR;
	while (flags != AUDCLNT_BUFFERFLAGS_SILENT)
	{
		// Wait for next buffer event to be signaled.
		DWORD retval = WaitForSingleObject(lpHandles[0], 2000);
		switch (retval)
		{
		// Capture was signaled
		case WAIT_OBJECT_0:
			//cout << "Capture\n" << flush;
			// Grab the next empty buffer from the audio device.
			hr = pCaptureClient->GetBuffer(
				&pDataC,
				&numFramesToRead,
				&pdwFlags,
				&i1,
				&i2
			);
			EXIT_ON_ERROR(hr)
			//work
			memcpy(pData, pDataC, 2 * numFramesToRead * sizeof(BYTE));
			hr = pCaptureClient->ReleaseBuffer(numFramesToRead);
			EXIT_ON_ERROR(hr)

			if (!started) {
				hr = pAudioClientR->Start();  // Start playing.
				EXIT_ON_ERROR(hr)
					started = true;
			}

			break;
		case WAIT_TIMEOUT:
			printf("Wait timed out.\n");
			pAudioClientR->Stop();
			pAudioClientC->Stop();
			hr = ERROR_TIMEOUT;
			goto Exit;
			// Return value is invalid.
		default:
			printf("Wait error: %d\n", GetLastError());
			ExitProcess(0);
		}

		DWORD retval2 = WaitForMultipleObjects(2, &lpHandles[1], FALSE, 2000);
		switch (retval2)
		{
		// Render was signaled
		case WAIT_OBJECT_0:
			//cout << "Render\n" << flush;
			// Grab the next empty buffer from the audio device.
			hr = pRenderClient->GetBuffer(bufferFrameCountR, &pDataR);
			EXIT_ON_ERROR(hr)

			//work
			inputBuffer = PCMFunctions::byteToDouble(pData);
			buffL = ConvolverPrime::longConvolve(inputBuffer, 0);
			buffR = ConvolverPrime::longConvolve(inputBuffer, 1);

			tempL = PCMFunctions::doubleToByte(buffL);
			tempR = PCMFunctions::doubleToByte(buffR);
			for (int i = 0; i < bufferFrameCountR; i++) {
				pDataR[4 * i] = tempL[2*i];
				pDataR[4 * i + 1] = tempL[2*i+1];
				pDataR[4 * i + 2] = tempR[2 * i];
				pDataR[4 * i + 3] = tempR[2 * i+1];
			}
			////

			hr = pRenderClient->ReleaseBuffer(bufferFrameCountR, flags);
			EXIT_ON_ERROR(hr)

			break;
		// Keyboard input was signalled.
		case WAIT_OBJECT_0 + 1:
			DWORD count;
			ReadConsoleInput(lpHandles[2], &event, 1, &count);
			if ((event.EventType == KEY_EVENT)
				&& !event.Event.KeyEvent.bKeyDown)
				if (event.Event.KeyEvent.wVirtualKeyCode == 0x51) {
					goto Exit;
				}
			break;
		case WAIT_TIMEOUT:
			printf("Wait timed out.\n");
			pAudioClientR->Stop();
			pAudioClientC->Stop();
			hr = ERROR_TIMEOUT;
			goto Exit;
			// Return value is invalid.
		default:
			printf("Wait error: %d\n", GetLastError());
			ExitProcess(0);
		}
	}

	// Wait for the last buffer to play before stopping.
	Sleep((DWORD)(phnsMinimumDevicePeriodR / REFTIMES_PER_MILLISEC));

	hr = pAudioClientR->Stop();  // Stop playing.
	EXIT_ON_ERROR(hr)

	Exit:
	if (lpHandles[0] != NULL)
	{
		CloseHandle(lpHandles[0]);
	}
	if (lpHandles[1] != NULL)
	{
		CloseHandle(lpHandles[1]);
	}
	if (hTask != NULL)
	{
		AvRevertMmThreadCharacteristics(hTask);
	}
	CoTaskMemFree(pwfx);
	SAFE_RELEASE(pEnumerator)
	SAFE_RELEASE(pDeviceR)
	SAFE_RELEASE(pAudioClientR)
	SAFE_RELEASE(pRenderClient)

	return hr;
}

int main() {
	int *numSamples = new int();
	double** BRIR = LoadBRIR::load(numSamples);
	int BRIRLength = *numSamples;
	ConvolverPrime::init(BRIR, *numSamples);

	CoInitialize(nullptr);
	return convolveVoice();
	CoUninitialize();
}

