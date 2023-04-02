#pragma once

class AntiJitterFilter {
	/*
	* Helps supress cracking noise from potentiometers. If the newly fed value differs insignificantly
	* from the past value, the past value will be returned. If the difference is above threshold, then 
	* the past value will be updated with the new value and returned.
	*/
public:
	AntiJitterFilter(float threshold = 1.5f);

	float update(float value);

private:
	float mThreshold;
	float mLastValue = 0.f;
};