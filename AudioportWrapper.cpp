#include "AudioportWrapper.h"
#include <stdlib.h>
#include <stdio.h>
#include "portaudio.h"
#include "pa_win_wasapi.h"
#include "pa_win_wdmks.h"
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

#pragma comment(lib, "Avrt.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "portaudio_x64.lib")

typedef struct
{
	float left_phase;
	float right_phase;
}
paTestData;

double* buffL;
double* buffR;
double* inputBufferDouble;
BYTE* tempL;
BYTE* tempR;

static int paCallback(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData)
{
	/* Cast data passed through stream to our structure. */
	paTestData *data = (paTestData*)userData;
	float *out = (float*)outputBuffer;
	unsigned int i;

	inputBufferDouble = PCMFunctions::floatToDouble((float*)inputBuffer);
	buffL = ConvolverPrime::longConvolve(inputBufferDouble, 0);
	buffR = ConvolverPrime::longConvolve(inputBufferDouble, 1);
	for (int i = 0; i < framesPerBuffer; i++) {
		out[2 * i] = (float)buffL[i];
		out[2 * i + 1] = (float)buffR[i];
	}

	return 0;
}

void StartAudioLoopback() {
	Pa_Initialize();
	Pa_GetVersionText();
	int lastHost = Pa_GetHostApiCount() - 1;
	Pa_GetHostApiInfo(lastHost);

	PaStream* pStream = 0;
	paTestData userData = { 0, 0 };

	Pa_OpenDefaultStream(&pStream,
		1,          /* no input channels */
		2,          /* stereo output */
		paFloat32,  /* 32 bit floating point output */
		48000,
		1024,        /* frames per buffer, i.e. the number
					of sample frames that PortAudio will
					request from the callback. Many apps
					may want to use
					paFramesPerBufferUnspecified, which
					tells PortAudio to pick the best,
					possibly changing, buffer size.*/
		paCallback, /* this is your callback function */
		&userData); /*This is a pointer that will be passed to
				your callback*/
	PCMFunctions::init(1024);
	Pa_StartStream(pStream);
	Pa_Sleep(100000);
}

int AudioportWrapper::start()
{
	int *numSamples = new int();
	double** BRIR = LoadBRIR::load(numSamples);
	int BRIRLength = *numSamples;
	ConvolverPrime::init(BRIR, *numSamples);

	StartAudioLoopback();
	return 0;
}

