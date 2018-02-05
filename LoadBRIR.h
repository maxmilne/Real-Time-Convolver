//-----------------------------------------------------------
// Loads a BRIR (binaural room impulse response from a .wav 
// file. Function returns an array of two arrays, representing
// the floating point left and right channel PCM representations
// of the impulse response, respectively.
//-----------------------------------------------------------

#pragma once

#include <stdio.h>
#include <cstring>

class LoadBRIR
{
public:
	static double** load(int* outNumSamples);
};