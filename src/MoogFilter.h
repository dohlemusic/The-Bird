#pragma once

class MoogFilter
{
public:
	MoogFilter(float sampleRate);
	void init();
	void calc();
	float process(float input);
	float getCutoff() const;
	void setCutoff(float cutoffHz);
	float getRes() const;
	void setRes(float resonance);

protected:
	float mCutoff;
	float mResonance;
	float mSampleRate;
	float y1, y2, y3, y4;
	float mOldX;
	float mOldY1, mOldY2, mOldY3;
	float mX;
	float mR;
	float mP;
	float mK;
};