#include "LoadBRIR.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

double ** LoadBRIR::load(int* outNumSamples)
{
	unsigned int channels;
	unsigned int sampleRate;
	drwav_uint64 totalSampleCount;
	float* pSampleData = drwav_open_and_read_file_f32("brir.wav", &channels, &sampleRate, &totalSampleCount);
	if (pSampleData == NULL) {
		exit(1);
	}

	//output
	double **outp = new double*[channels];
	for (int i = 0; i < 2; i++) {
		outp[i] = new double[totalSampleCount/2];
	}

	for (int i = 0; i < totalSampleCount/2; i++) {
		outp[0][i] = pSampleData[2 * i] / 40;
		outp[1][i] = pSampleData[2 * i + 1] / 40;
	}

	*outNumSamples = totalSampleCount / 8;
	return outp;
}
