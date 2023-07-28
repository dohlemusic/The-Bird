#pragma once
#include <vector>

#include <DaisyDSP.h>

#include "RingBuffer.h"

static constexpr float SAMPLE_RATE = 48000;
static constexpr size_t AUDIO_BLOCK_SIZE = 360;

static constexpr size_t NUM_DELAYS = 4;
static constexpr size_t MAX_DELAY_LENGTH = 16384;
static constexpr size_t MAX_BUFFER_LENGTH = 6200;
float resizeOutputBuffer[MAX_BUFFER_LENGTH];

// the variables are not used in code, but they let user preview the delay properties in IDE or compile time
static constexpr float minDelayLengthSeconds = (MAX_DELAY_LENGTH / (float)SAMPLE_RATE) / (MAX_BUFFER_LENGTH / AUDIO_BLOCK_SIZE);
static constexpr float maxDelayLengthSeconds = (MAX_DELAY_LENGTH / (float)SAMPLE_RATE);
static constexpr float expansionFactor = maxDelayLengthSeconds / minDelayLengthSeconds;


// Very similar to tanh in <-1, 1> range and decaying to 0 outside of that range
inline float softClip(float sample)
{
	return sample - 0.333333f * sample * sample * sample;
}

float lerp(float x, float x0, float x1, float y0, float y1)
{
	return y0 + (x - x0) * ((y1 - y0) / (x1 - x0));
}

inline bool resizeNearestNeighbor(const float* current, size_t currentSize, float* out, size_t newSize)
{
	const float scaleFactor = static_cast<float>(currentSize) / static_cast<float>(newSize);
	for (size_t outIdx = 0; outIdx < newSize - 1; ++outIdx)
	{
		const float currentFractionalIdx = outIdx * scaleFactor;
		const int currentIdx = static_cast<size_t>(currentFractionalIdx);
		out[outIdx] = lerp(currentFractionalIdx, currentIdx, currentIdx + 1, current[currentIdx], current[currentIdx + 1]); // current[currentIdx]; 
	}

	out[newSize - 1] = current[currentSize - 1];
	return true;
}

class BucketBrigadeDelay 
{
public:
	BucketBrigadeDelay()
	{
		mFilter.Init(SAMPLE_RATE);
		setGain(0.5f);
		setLength(MAX_DELAY_LENGTH);
	}

	void update(const float* input, float* output, size_t blockSize)
	{
		// rather than changing the actual delay line length, we just resample
		// the input and output, simulating change of clock speed in BBD delay
		size_t newSize = (MAX_DELAY_LENGTH / mNewLength) * blockSize;		
		if(newSize >= MAX_BUFFER_LENGTH) {
			newSize = MAX_BUFFER_LENGTH -1;
		}
		if(newSize < blockSize) {
			newSize = blockSize;
		}

		const float inputOutputScaleFactor = static_cast<float>(blockSize) / static_cast<float>(newSize);
		for(int i=0; i<newSize - 1; ++i) {
			const float currentFractionalIdx = i * inputOutputScaleFactor;
			const int currentIdx = static_cast<size_t>(currentFractionalIdx);
			const float inputInterpolated = lerp(currentFractionalIdx, currentIdx, currentIdx + 1, input[currentIdx], input[currentIdx + 1]);
			updateDelay(inputInterpolated, resizeOutputBuffer, i);
		}
		// we can't go to the last index, because lerp would read outside of input array bounds, so the last element gets simply copied
		updateDelay(input[blockSize - 1], resizeOutputBuffer, newSize - 1);

		resizeNearestNeighbor(resizeOutputBuffer, newSize, output, blockSize);
	}

	void reset() {
		if(!mIsReset) {
			mDelay.reset();
			mIsReset = true;
		}
	}

    bool isReset() {
        return mIsReset;
    }

	void setCutoff(float cutoffFrequency)
	{
		mFilter.SetFreq(cutoffFrequency);
	}

	bool setGain(float gain) {
		if (gain > 1.1f) {
			return false;
		}

		mGain = gain;
		return true;
	}

	void setLength(float length)
	{
		mOldLength = mNewLength;
		mNewLength = length;
	}

private:

	void updateDelay(float input, float* output, size_t outputIndex) {
            float out = softClip(mDelay.read());
			out = mFilter.Process(out);
			mDelay.write(input + out * mGain);
			float average = (input + out) * .5f;

			output[outputIndex] = average;
		};

	bool mIsReset = true;
	daisysp::Tone mFilter;
	RingBuffer<float, MAX_DELAY_LENGTH> mDelay;
	float mGain = 0.95f;
	float mOldLength;
	float mNewLength;
	float mOffset;
};

class TankVerb
{
public:
	TankVerb() :
		mCurrentNumDelays(NUM_DELAYS),
		mMaxDelayNumber(NUM_DELAYS),
		mMaxRoomSize(MAX_DELAY_LENGTH),
		mCurrentRoomSize(MAX_DELAY_LENGTH)
	{
		setLength(MAX_DELAY_LENGTH);
	}

	void setNumDelays(unsigned int num_delays)
	{
		if (num_delays > mMaxDelayNumber)
		{
			return;
		}

		mCurrentNumDelays = num_delays;
	}

	void setCutoff(float cutoffFrequency)
	{
		for(auto& delay : mDelays)
		{
			delay.setCutoff(cutoffFrequency);
		}
	}

	void setLength(unsigned length)
	{
		if (length >= mMaxRoomSize)
		{
			length = mMaxRoomSize - 1;
		}

		//// disable one delay line at very short delay length to save CPU
		//if(length < 0.075f * MAX_DELAY_LENGTH) {
		//	mDelays.back().reset();
		//	setNumDelays(mMaxDelayNumber - 1);
		//}
		//else {
		//	setNumDelays(mMaxDelayNumber);
		//}
		
		mCurrentRoomSize = length;

		const unsigned scaleFactor = mMaxDelayNumber > 1u ? (length - mMinLength) / (mMaxDelayNumber - 1u) : 1u;
		for(int i=0; i<mMaxDelayNumber; ++i) {
			mDelays[i].setLength(mCurrentRoomSize - i * scaleFactor);
		}
	}

	void setGain(float gain)
	{
		for (unsigned int i = 0u; i < mMaxDelayNumber; ++i) {
			mDelays[i].setGain(gain);
		}
	}

	void setSpread(float spread)
	{
		if (spread < 0.f || spread > 1.f)
		{
			return;
		}

		mMinLength = spread * mCurrentRoomSize;
		if (mMinLength < AUDIO_BLOCK_SIZE)
		{
			mMinLength = AUDIO_BLOCK_SIZE;
		}
	}

	float tmp[AUDIO_BLOCK_SIZE];
	void update(const float* const input, float* output, unsigned count)
	{
		if (input == nullptr || output == nullptr || count != AUDIO_BLOCK_SIZE)
		{
			return;
		}

		memset(output, 0.f, AUDIO_BLOCK_SIZE * sizeof(float));
		const float denominator = 1.f / static_cast<float>(mCurrentNumDelays);
		for (size_t d = 0; d < mCurrentNumDelays; ++d)
		{
			mDelays[d].update(input, tmp, AUDIO_BLOCK_SIZE);

			for (size_t i = 0; i < AUDIO_BLOCK_SIZE; ++i)
			{
				output[i] += tmp[i] * denominator;
			}
		}
	}


private:
	unsigned int mCurrentNumDelays;
	unsigned int mMaxDelayNumber;
	unsigned int mMinLength = AUDIO_BLOCK_SIZE;
	size_t mMaxRoomSize;
	size_t mCurrentRoomSize;

	std::array<BucketBrigadeDelay, NUM_DELAYS> mDelays;
};
