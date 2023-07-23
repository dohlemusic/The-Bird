#include "RingBuffer.h"
#include <cmath>
#include <memory>

float lerp(float x, float x0, float x1, float y0, float y1)
{
	return y0 + (x - x0) * ((y1 - y0) / (x1 - x0));
}

inline bool resizeNearestNeighbor(const float* current, size_t currentSize, float* out, size_t newSize, size_t offset = 0u)
{
	if (currentSize == newSize)
	{
		return true;
	}

	const float scaleFactor = static_cast<float>(currentSize) / static_cast<float>(newSize);
	for (size_t outIdx = 0; outIdx < newSize; ++outIdx)
	{
		const float currentFractionalIdx = outIdx * scaleFactor;
		const int currentIdx = static_cast<size_t>(currentFractionalIdx);
		out[outIdx] = current[(currentIdx + offset) % currentSize]; // lerp(currentFractionalIdx, currentIdx, currentIdx + 1, current[currentIdx], current[currentIdx + 1]);
	}
	return true;
}

RingBuffer::RingBuffer(float* buffer, float* resamplingBuffer, size_t maxLength):
	mBuffer(buffer),
	mResamplingBuffer(resamplingBuffer),
	mMaxLength(maxLength),
	mCurrentLength(maxLength),
	mHead(maxLength - 1u),
	mTail(0)
{}

void RingBuffer::resize(size_t newLength)
{
	if (newLength == mCurrentLength)
	{
		return;
	}

	if (newLength >= mMaxLength) {
		newLength = mMaxLength - 1;
	}

	bool shouldUpdate = false;

	if (mResamplingBuffer != nullptr)
	{
		resizeNearestNeighbor(mBuffer, mCurrentLength, mResamplingBuffer, newLength, mHead);
		shouldUpdate = true;
	}

	mTail = mTail % newLength;
	mHead = (mTail + newLength) % newLength;
	mCurrentLength = newLength;

	if (shouldUpdate)
	{
		update(mResamplingBuffer, newLength);
	}
}

void RingBuffer::update(const float* input, size_t blockSize)
{
	for (size_t i = 0; i < blockSize; ++i)
	{
		mBuffer[mHead] = input[i];
		mHead = (mHead + 1u) % mCurrentLength;
		mTail = (mTail + 1u) % mCurrentLength;
	}
}

void RingBuffer::get(float* output, size_t blockSize, float samplesOffset) const
{
	// Yep, fractional length is possible :) This is useful for variations of Karplus Strong model (keytar) algorithms, time-stretching based pitch shift etc.
	double integerPart;
	const float fractionalPart = modf(samplesOffset, &integerPart);

	for (auto i = static_cast<size_t>(samplesOffset); i < blockSize; ++i)
	{
		output[i - static_cast<size_t>(samplesOffset)] = (1.f - fractionalPart) * mBuffer[(mTail + i) % mCurrentLength] + (fractionalPart)*mBuffer[(mTail + i + 1u) % mCurrentLength];
	}
}

void RingBuffer::get(float* output, size_t blockSize, size_t samplesOffset) const
{
	for (auto i = samplesOffset; i < blockSize; ++i)
	{
		output[i - samplesOffset] = mBuffer[(mTail + i) % mCurrentLength];
	}
}
