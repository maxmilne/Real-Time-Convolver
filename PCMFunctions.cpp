#include "PCMFunctions.h"

double* PCMFunctions::outStatic;
int PCMFunctions::buffSizeStatic;
float* PCMFunctions::conversionArrayStatic;
WORD* PCMFunctions::conversionArray2Static;

void PCMFunctions::init(int buffSize) {
	outStatic = new double[buffSize];
	buffSizeStatic = buffSize;
	conversionArrayStatic = new float[buffSizeStatic];
	conversionArray2Static = new WORD[buffSizeStatic];
}

double* PCMFunctions::byteToDouble(BYTE* buff) {
	for (int i = 0; i < buffSizeStatic; i++) {
		outStatic[i] = (float)(((short*)buff)[i] / 32768.0f);
	}
	return outStatic;
}

double* PCMFunctions::floatToDouble(float* buff) {
	for (int i = 0; i < buffSizeStatic; i++) {
		outStatic[i] = (double)buff[i];
	}
	return outStatic;
}

BYTE* PCMFunctions::doubleToByte(double* buff) {
	for (int i = 0; i < buffSizeStatic; i++) {
		conversionArrayStatic[i] = buff[i] * 1.0;
		conversionArray2Static[i] = (WORD)((short)(conversionArrayStatic[i] * 32768.0f));
	}
	return (BYTE*)conversionArray2Static;
	//memcpy(_pTheirCurrentBuffer, conversionArray2Static, buffSizeStatic * sizeof(WORD));
}
