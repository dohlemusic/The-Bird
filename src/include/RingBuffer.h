#pragma once

#include <strings.h>

class RingBuffer
{
public:
	RingBuffer(float* buffer, float* resamplingBuffer, size_t maxLength);

	void resize(size_t newLength);

	void update(const float* input, size_t blockSize);

	void get(float* output, size_t blockSize, float samplesOffset) const;

	void get(float* output, size_t blockSize, size_t samplesOffset = 0) const;

private:
	float* mBuffer = nullptr;
	float* mResamplingBuffer = nullptr;

	size_t mMaxLength;
	size_t mCurrentLength;
	size_t mHead;
	size_t mTail;
};