#pragma once
#include <vector>

#include <DaisyDSP.h>

static constexpr float SAMPLE_RATE = 48000;
static constexpr size_t AUDIO_BLOCK_SIZE = 256;

static constexpr size_t NUM_DELAYS = 4;
static constexpr size_t MAX_DELAY_LENGTH = 16384;
static constexpr size_t MIN_DELAY_LENGTH = 16;

// Very similar to tanh in <-1, 1> range and decaying to 0 outside of that range
inline float softClip(float sample)
{
	return sample - 0.333333f * sample * sample * sample;
}

float lerp(float x, float y0, float y1)
{
	return y0 + x * ((y1 - y0));
}

// unless preventOverrun is set to true, current array size must be at least 1 bigger than currentSize
// otherwise, the last iteration of the resize loop will try to access an element outside the input array bounds to interpolate
// preventOverrun prevents this by iterating until currentSize-1 and copying the last element of current into the last element of out
inline bool resizeNearestNeighbor(const float *current, size_t currentSize, float *out, size_t newSize)
{
	const float scaleFactor = static_cast<float>(currentSize) / static_cast<float>(newSize);

	for (size_t outIdx = 0; outIdx < newSize; ++outIdx)
	{
		const float currentFractionalIdx = outIdx * scaleFactor;
		const int currentIdx = static_cast<size_t>(currentFractionalIdx);
		out[outIdx] = lerp(currentFractionalIdx - currentIdx, current[currentIdx], current[currentIdx + 1]);
	}

	return true;
}

class BucketBrigadeDelay
{
public:
	BucketBrigadeDelay()
	{
		mFilter.Init(SAMPLE_RATE);
		setGain(0.5f);
		setLength(MAX_DELAY_LENGTH - 4);
		mDelay = mTarget;
	}

	// length in samples at the audio rate
	void setLength(float samples)
	{
		mTarget = std::clamp(samples, kMinDelay, (float)(BUF_SIZE - 4));
	}

	void setCutoff(float hz) { mUserCutoff = hz; }

	bool setGain(float g)
	{
		if (g > 1.1f) return false;
		mGain = g;
		return true;
	}

	void update(const float *input, float *output, size_t blockSize)
	{
		updateFilter(); // once per block is plenty

		for (size_t i = 0; i < blockSize; ++i)
		{
			// Slew-limited delay time: this IS the BBD pitch bend.
			float step = (mTarget - mDelay) * kSlew;
			mDelay += std::clamp(step, -kMaxStep, kMaxStep);

			float wet = readInterp(mDelay);
			wet = softClip(wet);
			wet = mFilter.Process(wet);

			mBuf[mWrite] = input[i] + wet * mGain;
			mWrite = (mWrite + 1) & MASK;

			output[i] = 0.5f * (input[i] + wet);
		}
		mIsReset = false;
	}

	void reset()
	{
		if (mIsReset) return;
		std::fill(std::begin(mBuf), std::end(mBuf), 0.f);
		mFilter.Init(SAMPLE_RATE);
		mIsReset = true;
	}

private:
	static constexpr size_t BUF_SIZE = MAX_DELAY_LENGTH; // must be power of two
	static constexpr size_t MASK = BUF_SIZE - 1;
	static constexpr float kMinDelay = 2.f;
	static constexpr float kSlew = 0.0005f;             // ~40 ms time constant
	static constexpr float kMaxStep = 0.5f;             // caps bend at ~±1 octave
	static constexpr float kVirtualStages = 8192.f;     // "character", independent of buffer size

	static inline float softClip(float x)
	{
		x = std::clamp(x, -1.f, 1.f);
		return 1.5f * (x - x * x * x * (1.f / 3.f));
	}

	float readInterp(float delay) const
	{
		float pos = static_cast<float>(mWrite) - delay;
		if (pos < 0.f) pos += BUF_SIZE;
		const size_t i0 = static_cast<size_t>(pos);
		const float frac = pos - static_cast<float>(i0);
		return lerp(frac, mBuf[i0 & MASK], mBuf[(i0 + 1) & MASK]);
	}

	void updateFilter()
	{
		float cutoff = mUserCutoff; 
		mFilter.SetFreq(cutoff);
	}

	daisysp::Tone mFilter;
	float mBuf[BUF_SIZE] = {};
	size_t mWrite = 0;
	float mDelay = 0, mTarget = 0;
	float mGain = 0.5f;
	float mUserCutoff = 20000.f;
	bool mIsReset = true;
};

class TankVerb
{
public:
	TankVerb() : mCurrentNumDelays(NUM_DELAYS),
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
		for (auto &delay : mDelays)
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

		mCurrentRoomSize = length;

		const unsigned scaleFactor = mMaxDelayNumber > 1u ? (length - mMinLength) / (mMaxDelayNumber - 1u) : 1u;
		for (int i = 0; i < mMaxDelayNumber; ++i)
		{
			mDelays[i].setLength(mCurrentRoomSize - i * scaleFactor);
		}
	}

	void setGain(float gain)
	{
		for (unsigned int i = 0u; i < mMaxDelayNumber; ++i)
		{
			mDelays[i].setGain(gain);
		}
	}

	void setSpread(float spread)
	{
		if (spread > 1.0)
			spread = 1.0;
		if (spread < 0.0)
			spread = 0.0;

		mMinLength = spread * mCurrentRoomSize;
	}

	float tmp[AUDIO_BLOCK_SIZE];
	void update(const float *const input, float *output, unsigned count)
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
