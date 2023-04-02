#pragma once
#include <vector>

#include "IIRLowPass.h"
#include "MoogFilter.h"
#include "RingBuffer.h"
#include "so_butterworth_lpf.h"

static constexpr size_t AUDIO_BLOCK_SIZE = 480;
static constexpr size_t NUM_DELAYS = 4;
static constexpr size_t MAX_DELAY_LENGTH = 20000;
inline float DSY_SDRAM_BSS kDelayBufferss[NUM_DELAYS][MAX_DELAY_LENGTH];
inline float kResamplingBuffer[MAX_DELAY_LENGTH + 1];

template<class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi)
{
	return v <= lo ? lo : v >= hi ? hi : v;
}

// Fast tanh from https://varietyofsound.wordpress.com/2011/02/14/efficient-tanh-computation-using-lamberts-continued-fraction/
inline float softClip(float sample)
{
	float x2 = sample * sample;
	float a = sample * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
	float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
	return a / b;
}


class Delay
{
public:
	Delay(unsigned int index, float maxLength, float gain, float* resamplingBuffer) :
		mBuffer(RingBuffer(kDelayBufferss[index], resamplingBuffer, static_cast<size_t>(maxLength + 1)))
	{
		setGain(gain);
		setLength(maxLength);
	}

	void update(const float* input, float* output, size_t blockSize)
	{
		float tmp[AUDIO_BLOCK_SIZE + 1];

		mBuffer.get(tmp, AUDIO_BLOCK_SIZE + 1u, mOffset);

		// feedback loop
		for (size_t i = 0; i < blockSize; ++i)
		{
			const float left = tmp[i];
			const float right = tmp[i + 1];
			const float average = input[i] + mGain * mFilter.update((left + right) / 2.f);
			output[i] = softClip(average);
		}

		mBuffer.update(output, blockSize);
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
		mLength = length;
		const auto lengthInt = static_cast<size_t>(mLength);

		mBuffer.resize(lengthInt + 1);
		mOffset = mLength - lengthInt;
	}

private:
	DSPFilters::Butterworth mFilter{20000.f, 48000.f};
	RingBuffer mBuffer;
	float mGain = 0.95f;
	float mLength;
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
			mDelays.emplace_back(i, MAX_DELAY_LENGTH, gain, kResamplingBuffer);
		}
		setLength(500);
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

	std::vector<Delay> mDelays;
};
