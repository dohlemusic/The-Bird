#pragma once
#include <vector>

#include <DaisyDSP.h>

#include "filters/ButterworthLPF.h"

static constexpr size_t AUDIO_BLOCK_SIZE = 360;
static constexpr size_t NUM_DELAYS = 4;
static constexpr size_t MAX_DELAY_LENGTH = 16000;
static constexpr size_t MAX_BUFFER_LENGTH = 4200;
float resizeInputBuffer[MAX_BUFFER_LENGTH];
float resizeOutputBuffer[MAX_BUFFER_LENGTH];

// Fast tanh from https://varietyofsound.wordpress.com/2011/02/14/efficient-tanh-computation-using-lamberts-continued-fraction/
inline float softClip(float sample)
{
	float x2 = sample * sample;
	float a = sample * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
	float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
	return a / b;
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
	BucketBrigadeDelay(float length, float gain)
	{
		mDelay.Init();
		setGain(gain);
		setLength(length);
		mDelay.SetDelay(MAX_DELAY_LENGTH);
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
			float out = mFilter.update(softClip(mDelay.Read()));
			mDelay.Write(resizeInputBuffer[i] + out * mGain);
			float average = (resizeInputBuffer[i] + out) * .5f;

			resizeOutputBuffer[i] = average;
		}
		resizeNearestNeighbor(resizeOutputBuffer, newSize, output, blockSize);
	}

	void setCutoff(float cutoffFrequency)
	{
		mFilter.setCutoff(cutoffFrequency);
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
	DSPFilters::Butterworth mFilter{20000.f, 48000.f};
	DelayLine<float, MAX_DELAY_LENGTH> mDelay;
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
		for (unsigned int i = 0u; i < mCurrentNumDelays; ++i)
		{
			constexpr auto gain = 0.9f;
			mDelays.emplace_back(MAX_DELAY_LENGTH, gain);
		}
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
		if (length > mMaxRoomSize)
		{
			return;
		}
		mCurrentRoomSize = length;

		const unsigned scaleFactor = mMaxDelayNumber > 1u ? (length - mMinLength) / (mMaxDelayNumber - 1u) : 1u;

		// avoid resizing all at once to save performance
		mDelays[mIndexToResize].setLength(mCurrentRoomSize - mIndexToResize * scaleFactor);
		mIndexToResize = (mIndexToResize + 1) % mCurrentNumDelays;
	}

	void setGain(float gain)
	{
		for (unsigned int i = 0u; i < mCurrentNumDelays; ++i) {
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
		//setLength(mCurrentRoomSize);
	}

	void update(const float* const input, float* output, unsigned count)
	{
		if (input == nullptr || output == nullptr || count != AUDIO_BLOCK_SIZE)
		{
			return;
		}

		float sum[AUDIO_BLOCK_SIZE] = { 0 };

		for (size_t d = 0; d < mMaxDelayNumber; ++d)
		{
			float tmp[AUDIO_BLOCK_SIZE];
			mDelays[d].update(input, tmp, AUDIO_BLOCK_SIZE);

			for (size_t i = 0; i < AUDIO_BLOCK_SIZE; ++i)
			{
				sum[i] += tmp[i] / static_cast<float>(mMaxDelayNumber);
			}
		}


		for (size_t i = 0; i < AUDIO_BLOCK_SIZE; ++i)
		{
			output[i] = sum[i];
		}
	}


private:
	unsigned int mCurrentNumDelays;
	unsigned int mMaxDelayNumber;
	unsigned int mMinLength = AUDIO_BLOCK_SIZE;
	size_t mIndexToResize = 0;
	size_t mMaxRoomSize;
	size_t mCurrentRoomSize;

	std::vector<BucketBrigadeDelay> mDelays;
};