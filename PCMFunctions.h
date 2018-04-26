//-----------------------------------------------------------
// Contains functions to handle conversion between integer and
// floating point PCM representations.
//-----------------------------------------------------------

#include <string>
#include "Windows.h"

#pragma once
#pragma comment(lib, "ole32.lib")

using namespace std;

class PCMFunctions
{

public:
	static double* outStatic;
	static int buffSizeStatic;
	static float* conversionArrayStatic;
	static WORD* conversionArray2Static;

	static void init(int buffSize);
	static double* byteToDouble(BYTE* buff);
	static double* floatToDouble(float* buff);
	static BYTE* doubleToByte(double* buff);

};