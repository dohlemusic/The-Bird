#pragma once
#include <vector>

#include <DaisyDSP.h>

#include "RingBuffer.h"

static constexpr size_t AUDIO_BLOCK_SIZE = 360;
static constexpr size_t NUM_DELAYS = 4;
static constexpr size_t MAX_DELAY_LENGTH = 16384;
static constexpr size_t MAX_BUFFER_LENGTH = 5000;
float resizeInputBuffer[MAX_BUFFER_LENGTH];
float resizeOutputBuffer[MAX_BUFFER_LENGTH];

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
	for (size_t outIdx = 0; outIdx < newSize; ++outIdx)
	{
		const float currentFractionalIdx = outIdx * scaleFactor;
		const int currentIdx = static_cast<size_t>(currentFractionalIdx);
		out[outIdx] = lerp(currentFractionalIdx, currentIdx, currentIdx + 1, current[currentIdx], current[currentIdx + 1]); // current[currentIdx]; 
	}
	return true;
}

class BucketBrigadeDelay 
{
public:
	BucketBrigadeDelay()
	{
		mFilter.Init(48000);
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
		resizeNearestNeighbor(input, blockSize, resizeInputBuffer, newSize);
		Serial.print(newSize);
		for(int i=0; i<newSize; ++i) {
			float out = softClip(mDelay.read());
			out = mFilter.Process(out);
			mDelay.write(resizeInputBuffer[i] + out * mGain);
			float average = (resizeInputBuffer[i] + out) * .5f;
			resizeOutputBuffer[i] = average;
		}
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
