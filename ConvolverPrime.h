//-----------------------------------------------------------
// Handles multithreaded audio convolution logic.
// Algorithm based on William Gardner's 1995 paper:
// http://www.cs.ust.hk/mjg_lib/bibs/DPSu/DPSu.Files/Ga95.PDF
//-----------------------------------------------------------

#pragma once
#include <mutex>

class ConvolverThread {
	// The index of the thread, if the threads were ordered by 
	// the corresponding block order in the filter
	int index;

	// Is this thread for the convolution of the left or right channel?
	// 0 represents left, and 1 represents right.
	int channel;

	// Size of one of the blocks (this thread likely has two),
	// NOT including the zero padding for the FFT
	int blockSize;
	// Does it have only one block instead of two?
	// (this is the case if this is the last block
	// and there are an odd number of blocks)
	bool hasOnlyOneBlock;

	std::mutex* mut;
	ConvolverThread* threads;
	double* outputBuffer;

	std::condition_variable* acceptingWrites;
	std::condition_variable* writersFinished;
	std::condition_variable* acceptingStarts;
	bool* readersDone;
	bool* writersDone;
	bool* accWrites;
	bool* accStarts;

	// A copy of the grabbed input,
	// twice the size to make room for fft
	double* inputBlock;

	// The buffers holding the output data to be transferred for each block
	double* result1;
	double* result2;

	// Grab the input history from the static data
	void readStaticData();

	// Write results of convolution to outputBuffer
	// this entire function is atomic
	void writeResults(double* res1, double* res2);

public:
	ConvolverThread();
	ConvolverThread(int threadIndex, int chan);
	void run();

	// Is this thread currently due?
	bool due;
	// Is this thread currently due to read start data?
	bool startDue;
	// Is this thread currently done?
	bool done;
	// Is this thread now set to start up a new loop?
	bool starting;
	// Has the thread read its required data?
	bool started;
};

class ConvolverPrime {
public:
	static double* inputHistory;
	static int ihCircInd;
	static double* outputBufferL;
	static double* outputBufferR;
	static int obCircIndL;
	static int obCircIndR;
	// All data that encompasses BRIR (w/ preprocessing)
	static double** blocksL;
	static double** blocksR;
	static int numBlocks;

	// Length of the filter zero padded so that all blocks (which are powers of two) fit
	// Does not include zero padding of each individual block for FFT purposes
	static int filterBlocksTotalLength;

	static ConvolverThread* threadsL;
	static ConvolverThread* threadsR;
	static std::thread* stdThreadsL;
	static std::thread* stdThreadsR;
	static int numThreads;

	static double* inputBuffer;
	static double* outp;

	// Is the convolver still in the phase of loading in new threads into the system?
	static bool amStartupL;
	static bool amStartupR;
	static int startupNumL;
	static int startupNumR;
	static int convolveLoopIndL;
	static int convolveLoopIndR;
	static int convolveLoopLength;

	// Shared mutexes between all block processing threads and main longConvolve thread,
	// for both channels
	static std::mutex mutL;
	static std::mutex mutR;

	// Condition variables
	static std::condition_variable acceptingWritesL;
	static std::condition_variable acceptingWritesR;
	// Analogous boolean to the above
	static bool accWritesL;
	static bool accWritesR;


	static std::condition_variable writersFinishedL;
	static std::condition_variable writersFinishedR;
	// Analogous boolean to the above 
	static bool writersDoneL;
	static bool writersDoneR;

	static bool readersDoneL;
	static bool readersDoneR;

	static std::condition_variable acceptingStartsL;
	static std::condition_variable acceptingStartsR;
	// Analogous boolean to the above 
	static bool accStartsL;
	static bool accStartsR;

	// Channel == 0: left channel. Channel == 1: right channel.
	static double* longConvolve(double* input, int channel);

	static void shortConvolve(double* inputFFT, int filterSegment, int size, int channel, double*& out);
	static void complexMultiplyAndInv(double* in1, double* in2, int length, double*& out);
	static void shiftBuffer(double* buffer, int ind, int channel);
	static void splitBRIR(double** BRIR, int BRIRLength);
	static void init(double** BRIR, int length);

};

