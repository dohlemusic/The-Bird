#include "filters/AntiJitterFilter.h"

AntiJitterFilter::AntiJitterFilter(float threshold)
{
	mThreshold = threshold;
}

float AntiJitterFilter::update(float value)
{
	if (value > mLastValue + mThreshold || value < mLastValue - mThreshold) {
		mLastValue = value;
	}

	return mLastValue;
}
