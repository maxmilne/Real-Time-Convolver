#include "ConvolverPrime.h"
#include "fftsg_h.c"
#include <cmath>
#include <iostream>
#include <thread>
#include <math.h>
#include <Windows.h>

// Shared data ////
double* ConvolverPrime::inputHistory;
int ConvolverPrime::ihCircInd;
double* ConvolverPrime::outputBufferL;
double* ConvolverPrime::outputBufferR;
int ConvolverPrime::obCircIndL;
int ConvolverPrime::obCircIndR;
double** ConvolverPrime::blocksL;
double** ConvolverPrime::blocksR;
int ConvolverPrime::numBlocks;

int ConvolverPrime::filterBlocksTotalLength;

ConvolverThread* ConvolverPrime::threadsL;
ConvolverThread* ConvolverPrime::threadsR;
std::thread* ConvolverPrime::stdThreadsL;
std::thread* ConvolverPrime::stdThreadsR;
int ConvolverPrime::numThreads;

double* ConvolverPrime::inputBuffer;

bool ConvolverPrime::amStartupL;
bool ConvolverPrime::amStartupR;
int ConvolverPrime::startupNumL; 
int ConvolverPrime::startupNumR;
int ConvolverPrime::convolveLoopIndL; 
int ConvolverPrime::convolveLoopIndR;
int ConvolverPrime::convolveLoopLength;

std::mutex ConvolverPrime::mutL;
std::mutex ConvolverPrime::mutR;

std::condition_variable ConvolverPrime::acceptingWritesL;
std::condition_variable ConvolverPrime::acceptingWritesR;
std::condition_variable ConvolverPrime::writersFinishedL;
std::condition_variable ConvolverPrime::writersFinishedR;
std::condition_variable ConvolverPrime::acceptingStartsL;
std::condition_variable ConvolverPrime::acceptingStartsR;


bool ConvolverPrime::readersDoneL;
bool ConvolverPrime::readersDoneR;
bool ConvolverPrime::writersDoneL;
bool ConvolverPrime::writersDoneR;
bool ConvolverPrime::accWritesL;
bool ConvolverPrime::accWritesR;
bool ConvolverPrime::accStartsL;
bool ConvolverPrime::accStartsR;

static LARGE_INTEGER StartingTime, EndingTime, ElapsedMilliseconds;
static LARGE_INTEGER Frequency;

///////////////////

double* ConvolverPrime::longConvolve(double* inputBuffer, int channel) {
	std::mutex *mut;
	ConvolverThread* threads;
	std::thread* stdThreads;
	double* outputBuffer;
	int* obCircInd;

	std::condition_variable* acceptingWrites;
	std::condition_variable*writersFinished;
	std::condition_variable*acceptingStarts;
	bool* readersDone;
	bool* writersDone;
	bool* accWrites;
	bool* accStarts;

	bool* amStartup;
	int* startupNum;
	int* convolveLoopInd;
	if (channel == 0) {
		mut = &mutL;
		threads = threadsL;
		stdThreads = stdThreadsL;
		outputBuffer = outputBufferL;
		obCircInd = &obCircIndL;

		acceptingWrites = &acceptingWritesL;
		writersFinished = &writersFinishedL;
		acceptingStarts = &acceptingStartsL;
		readersDone = &readersDoneL;
		writersDone = &writersDoneL;
		accWrites = &accWritesL;
		accStarts = &accStartsL;

		amStartup = &amStartupL;
		startupNum = &startupNumL;
		convolveLoopInd = &convolveLoopIndL;
	} else {
		mut = &ConvolverPrime::mutR;
		threads = ConvolverPrime::threadsR;
		stdThreads = stdThreadsR;
		outputBuffer = outputBufferR;
		obCircInd = &obCircIndR;

		acceptingWrites = &acceptingWritesR;
		writersFinished = &writersFinishedR;
		acceptingStarts = &acceptingStartsR;
		readersDone = &readersDoneR;
		writersDone = &writersDoneR;
		accWrites = &accWritesR;
		accStarts = &accStartsR;

		amStartup = &amStartupR;
		startupNum = &startupNumR;
		convolveLoopInd = &convolveLoopIndR;
	}
	std::unique_lock<std::mutex> l(*mut);
	(*convolveLoopInd)++;
	if (*convolveLoopInd == convolveLoopLength) {
		*convolveLoopInd = 0;
		*amStartup = false;
	}
	if (channel == 0)
		shiftBuffer(inputHistory, 0, channel);
	shiftBuffer(outputBuffer, 1, channel);

	// Starting new convolutional loop
	if (channel == 0) {
		int dest = (ihCircInd - 64) % filterBlocksTotalLength;
		if (dest < 0) dest += filterBlocksTotalLength;
		memcpy(&inputHistory[dest], inputBuffer, 64 * sizeof(double));
	}

	if (*amStartup)
	{
		if ((*convolveLoopInd & (*convolveLoopInd + 1)) == 0 && *startupNum <= 8) {
			// It's time to spawn a new thread (power of two time slice)
			int threadIndex = (int)log2(*convolveLoopInd + 1);
			//std::thread t(&ConvolverThread::run, threads[threadIndex]);
			stdThreads[*startupNum] = std::thread(&ConvolverThread::run, &threads[threadIndex]);
			(*startupNum)++;
		}
	}

	for (int i = 0; i < numThreads; i++) {
		if ((*convolveLoopInd + 1) % (int)(pow(2, i)) == (int)(pow(2, i)) - 1 && i < *startupNum) {
			// Thread i is due to write to the output buffer this loop iteration.
			threads[i].due = true;
		}
		else {
			threads[i].due = false;
		}
	}

	for (int i = 0; i < numThreads; i++) {
		if ((*convolveLoopInd + 1) % (int)(pow(2, i)) == 0 && i < *startupNum) {
			// Thread i is due to start this loop iteration.
			threads[i].starting = true;
			threads[i].startDue = true;
		}
		else {
			threads[i].startDue = false;
		}
	}

	//QueryPerformanceFrequency(&Frequency);
	//QueryPerformanceCounter(&StartingTime);
	/// WAITING ON THREADS
	*accStarts = true;
	*readersDone = false;
	*writersDone = false;
	acceptingStarts->notify_one();
	int c = channel;
	writersFinished->wait(l, [c]() {
		if (c == 0) return readersDoneL && writersDoneL;
		else return readersDoneR && writersDoneR;
	});
	*accStarts = false;
	*accWrites = false;
	///
	//QueryPerformanceCounter(&EndingTime);
	//ElapsedMilliseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	//ElapsedMilliseconds.QuadPart *= 1000000;
	//ElapsedMilliseconds.QuadPart /= Frequency.QuadPart;
	//cout << ElapsedMilliseconds.QuadPart << "\n" << flush;

	for (int i = 0; i < numThreads; i++) {
		if (threads[i].due && i < *startupNum) {
			threads[i].started = false;
			threads[i].done = false;
		}
	}

	double * outp = new double[64];
	for (int i = 0; i < 64; i++) {
		outp[i] = outputBuffer[*obCircInd + i];
	}
	return outp;
}

// inputFFT: the already pre-FFT'd input segement. 
// filterSegment: Which segment of the filter are we convolving with? h0? h1? h2? etc.
// size: how big is the filter block without the padding?
// channel: 0 = left channel, 1 = right channel
// out: the output buffer
void ConvolverPrime::shortConvolve(double* inputFFT, int filterSegment, int size, int channel, double*& out) {
	double** blocks;
	if (channel) blocks = blocksR;
	else blocks = blocksL;

	complexMultiplyAndInv(inputFFT, blocks[filterSegment], size * 2, out);
}

void ConvolverPrime::complexMultiplyAndInv(double* in1, double* in2, int length, double*& out)
{
	out[0] = in1[0] * in2[0];
	out[1] = in1[1] * in2[1];
	for (int i = 1; i < length / 2; i++) {
		out[2 * i] = (in1[2 * i] * in2[2 * i]) - (in1[2 * i + 1] * in2[2 * i + 1]);
		out[2 * i + 1] = (in1[2 * i + 1] * in2[2 * i]) + (in1[2 * i] * in2[2 * i + 1]);
	}
	rdft(length, -1, out);
	for (int i = 0; i < length; i++) {
		out[i] *= 2.0 / length;
	}
}

// Shifts buffer left by 64 (end becomes zero padded)
// ind == 0 => inputHistory. ind == 1 => outputBuffer
void ConvolverPrime::shiftBuffer(double* buffer, int ind, int channel) {
	int* obCircInd;
	if (channel == 0)
		obCircInd = &obCircIndL;
	else
		obCircInd = &obCircIndR;
	if (ind == 0) {
		memset(&buffer[ihCircInd % filterBlocksTotalLength], 0, sizeof(double) * 64);
		ihCircInd = (ihCircInd + 64) % filterBlocksTotalLength;
	}
	else {
		memset(&buffer[*obCircInd % filterBlocksTotalLength], 0, sizeof(double) * 64);
		*obCircInd = (*obCircInd + 64) % filterBlocksTotalLength;
	}
}

void ConvolverPrime::splitBRIR(double** BRIR, int BRIRLength) {
	int i = 0;
	int size = 1;
	int sizeLowerBound = 0;
	while (true) {
		// Not multiplied by 2 to account for zero padding yet,
		// because the blocks will need to be doubled but this is only 
		// for enumerating how many blocks we need first
		sizeLowerBound += (64 * pow(2, i));
		if (BRIRLength > sizeLowerBound) size++;
		else break;

		sizeLowerBound += (64 * pow(2, i));
		if (BRIRLength > sizeLowerBound) size++;
		else break;
		i++;
	}

	numBlocks = size;
	numThreads = numBlocks % 2 == 0 ? numBlocks / 2 : numBlocks / 2 + 1;

	blocksL = new double*[size];
	int blockSize = 0;
	int blockIndex = 0;
	for (int i = 0; i < size; i++) {
		blockSize = 64 * pow(2, (i / 2)) * 2;
		blocksL[i] = new double[blockSize];
		for (int j = 0; j < blockSize; j++) {
			if (j < blockSize / 2 && blockIndex + j < BRIRLength) blocksL[i][j] = BRIR[0][blockIndex + j];
			else blocksL[i][j] = 0;
		}
		rdft(blockSize, 1, blocksL[i]);
		blockIndex += blockSize / 2;
	}

	blocksR = new double*[size];
	blockSize = 0;
	blockIndex = 0;
	for (int i = 0; i < size; i++) {
		blockSize = 64 * pow(2, (i / 2)) * 2;
		blocksR[i] = new double[blockSize];
		for (int j = 0; j < blockSize; j++) {
			if (j < blockSize / 2 && blockIndex + j < BRIRLength) blocksR[i][j] = BRIR[0][blockIndex + j];
			else blocksR[i][j] = 0;
		}
		rdft(blockSize, 1, blocksR[i]);
		blockIndex += blockSize / 2;
	}
}

// Initialize the convolver. BRIR assumed to be binaural.
void ConvolverPrime::init(double** BRIR) {
	splitBRIR(BRIR, 55125);
	filterBlocksTotalLength = 0;
	for (int i = 0; i < numBlocks; i++) {
		filterBlocksTotalLength += 64 * pow(2, (i / 2));
	}
	inputHistory = new double[filterBlocksTotalLength];
	for (int i = 0; i < filterBlocksTotalLength; i++) {
		inputHistory[i] = 0;
	}
	ihCircInd = 0;
	outputBufferL = new double[filterBlocksTotalLength];
	for (int i = 0; i < filterBlocksTotalLength; i++) {
		outputBufferL[i] = 0;
	}
	outputBufferR = new double[filterBlocksTotalLength];
	for (int i = 0; i < filterBlocksTotalLength; i++) {
		outputBufferR[i] = 0;
	}
	obCircIndL = filterBlocksTotalLength - 64;
	obCircIndR = filterBlocksTotalLength - 64;
	threadsL = new ConvolverThread[numThreads];
	for (int i = 0; i < numThreads; i++) {
		threadsL[i] = *(new ConvolverThread(i, 0));
	}
	threadsR = new ConvolverThread[numThreads];
	for (int i = 0; i < numThreads; i++) {
		threadsR[i] = *(new ConvolverThread(i, 1));
	}
	stdThreadsL = new std::thread[numThreads];
	for (int i = 0; i < numThreads; i++) {
		stdThreadsL[i] = std::thread();
	}
	stdThreadsR = new std::thread[numThreads];
	for (int i = 0; i < numThreads; i++) {
		stdThreadsR[i] = std::thread();
	}

	amStartupL = true;
	amStartupR = true;
	startupNumL = 0;
	startupNumR = 0;
	convolveLoopIndL = -1;
	convolveLoopIndR = -1;
	convolveLoopLength = filterBlocksTotalLength / 64;
	readersDoneL = false;
	readersDoneR = false;
	writersDoneL = false;
	writersDoneR = false;
	accWritesL = false;
	accWritesR = false;
	accStartsL = false;
	accStartsR = false;

}

/////////////////////////////////////

ConvolverThread::ConvolverThread()
{
}


// index: the index of the thread, if the threads were ordered by 
// the corresponding block order in the filter
ConvolverThread::ConvolverThread(int threadIndex, int chan)
{
	index = threadIndex;
	channel = chan;

	// We don't multiply by two here because we just mean the block size
	// without the padding
	blockSize = 64 * pow(2, (index));
	if (index == ConvolverPrime::numBlocks - 1 && ConvolverPrime::numBlocks % 2 == 1) hasOnlyOneBlock = true;

	// TODO: ADD IFONLYONEBLOCK LOGIC
	//filterBlock1 = new double[blockSize * 2];
	//filterBlock2 = new double[blockSize * 2];
	//memcpy(filterBlock1, ConvolverPrime::blocksR[2 * index], blockSize * 2 * sizeof(double));
	//memcpy(filterBlock2, ConvolverPrime::blocksR[2 * index + 1], blockSize * 2 * sizeof(double));

	if (channel == 0) {
		mut = &ConvolverPrime::mutL;
		threads = ConvolverPrime::threadsL;
		outputBuffer = ConvolverPrime::outputBufferL;

		acceptingWrites = &ConvolverPrime::acceptingWritesL;
		writersFinished = &ConvolverPrime::writersFinishedL;
		acceptingStarts = &ConvolverPrime::acceptingStartsL;
		readersDone = &ConvolverPrime::readersDoneL;
		writersDone = &ConvolverPrime::writersDoneL;
		accWrites = &ConvolverPrime::accWritesL;
		accStarts = &ConvolverPrime::accStartsL;
	}
	else {
		mut = &ConvolverPrime::mutR;
		threads = ConvolverPrime::threadsR;
		outputBuffer = ConvolverPrime::outputBufferR;

		acceptingWrites = &ConvolverPrime::acceptingWritesR;
		writersFinished = &ConvolverPrime::writersFinishedR;
		acceptingStarts = &ConvolverPrime::acceptingStartsR;
		readersDone = &ConvolverPrime::readersDoneR;
		writersDone = &ConvolverPrime::writersDoneR;
		accWrites = &ConvolverPrime::accWritesR;
		accStarts = &ConvolverPrime::accStartsR;
	}

	// Prepare input block
	inputBlock = new double[blockSize * 2];
	for (int i = 0; i < blockSize * 2; i++) {
		inputBlock[i] = 0;
	}

	due = false;
	done = false;
	starting = false;
	started = false;
	startDue = false;

	result1 = new double[blockSize * 2];
	result2 = new double[blockSize * 2];
}

void ConvolverThread::readStaticData() {
	std::unique_lock<std::mutex> l(*mut);

	int ind = (ConvolverPrime::ihCircInd - blockSize) % ConvolverPrime::filterBlocksTotalLength;
	if (ind < 0) {
		ind += ConvolverPrime::filterBlocksTotalLength;
	}
	if (ind + blockSize <= ConvolverPrime::filterBlocksTotalLength) {
		memcpy(inputBlock, &ConvolverPrime::inputHistory[ind], blockSize * sizeof(double));
	}
	else {
		int length1 = ConvolverPrime::filterBlocksTotalLength - ind;
		int length2 = blockSize - length1;
		memcpy(inputBlock, &ConvolverPrime::inputHistory[ind], length1 * sizeof(double));
		memcpy(&inputBlock[length1], &ConvolverPrime::inputHistory[0], length2 * sizeof(double));
	}
	memset(&inputBlock[blockSize], 0, blockSize * sizeof(double));

	done = false;

	// Reading is done for this thread's current cycle
	started = true;

	// Check if other threads need to read the data this cycle
	bool allReadersDone = true;
	for (int i = 0; i < ConvolverPrime::numThreads; i++) {
		if (threads[i].startDue && !(threads[i].started)) {
			allReadersDone = false;
			break;
		}
	}
	*readersDone = allReadersDone;
	*accWrites = allReadersDone;
	if (allReadersDone) acceptingWrites->notify_all();
	else if (!allReadersDone) acceptingStarts->notify_one();
}

void ConvolverThread::writeResults(double* res1, double* res2) {
	std::unique_lock<std::mutex> l(*mut);
	
	int c = channel;
	if (!*accWrites)
		acceptingWrites->wait(l, [c]() {
			if (c == 0) return (ConvolverPrime::accWritesL); 
			else return (ConvolverPrime::accWritesR);
	});

	// TODO: Test with "due" variable.
	int interval = pow(2, index);
	int convolveLoopInd;
	if (c == 0)
		convolveLoopInd = ConvolverPrime::convolveLoopIndL;
	else
		convolveLoopInd = ConvolverPrime::convolveLoopIndR;
	if ((convolveLoopInd + 1) % interval != interval - 1) {
		// This thread is done early, before it is due to be written, and must wait
		acceptingWrites->wait(l, [interval, c]() {
			if (c== 0) return (ConvolverPrime::convolveLoopIndL + 1) % interval == interval - 1; 
			else return (ConvolverPrime::convolveLoopIndR + 1) % interval == interval - 1;
		});
	}

	int* obCircInd;
	if (channel == 0)
		obCircInd = &ConvolverPrime::obCircIndL;
	else
		obCircInd = &ConvolverPrime::obCircIndR;
	for (int i = 0; i < blockSize * 2; i++) {
		if (blockSize > 10000) {
			blockSize *= 1;
		}
		outputBuffer[(*obCircInd + i) % ConvolverPrime::filterBlocksTotalLength] += res1[i];
		outputBuffer[(*obCircInd + blockSize + i) % ConvolverPrime::filterBlocksTotalLength] += res2[i];
	}

	// Writing is done for this thread's current cycle
	done = true;

	// Check if other threads need to write data this cycle
	bool allWritersDone = true;
	for (int i = 0; i < ConvolverPrime::numThreads; i++) {
		if (threads[i].due && !(threads[i].done)) {
			// cout << "wait!";
			allWritersDone = false;
			break;
		}
	}
	*writersDone = allWritersDone;
	*accWrites = !allWritersDone;
	if (allWritersDone) writersFinished->notify_one();
	else if (!allWritersDone) acceptingWrites->notify_one();

	////////// START ///
	int i = index;
	acceptingStarts->wait(l, [i, c]() {
		if (c == 0) return (ConvolverPrime::accStartsL && !ConvolverPrime::threadsL[i].done);
		else return (ConvolverPrime::accStartsR && !ConvolverPrime::threadsR[i].done);
	});
	starting = false;
	for (int i = 0; i < ConvolverPrime::numThreads; i++) {
		if (threads[i].starting) {
			acceptingStarts->notify_one();
			return;
		}
	}
}

void ConvolverThread::run() {
	while (true) {
		readStaticData();

		// Starting actual convolution


		rdft(blockSize * 2, 1, inputBlock);

		ConvolverPrime::shortConvolve(inputBlock, index * 2, blockSize, channel, result1);

		ConvolverPrime::shortConvolve(inputBlock, index * 2 + 1, blockSize, channel, result2);

		// Ending actual convolution

		writeResults(result1, result2);
	}
}