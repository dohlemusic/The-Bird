#include "MoogFilter.h"

MoogFilter::MoogFilter(float sampleRate) :
	mCutoff(sampleRate),
	mResonance(0),
	mSampleRate(sampleRate) {
	init();
}

void MoogFilter::init()
{
	// initialize values
	y1 = y2 = y3 = y4 = mOldX = mOldY1 = mOldY2 = mOldY3 = 0;
	calc();
};

void MoogFilter::calc()
{
	float f = (mCutoff + mCutoff) / mSampleRate; //[0 - 1]
	mP = f * (1.8f - 0.8f * f);
	mK = mP + mP - 1.f;

	float t = (1.f - mP) * 1.386249f;
	float t2 = 12.f + t * t;
	mR = mResonance * (t2 + 6.f * t) / (t2 - 6.f * t);
};

float MoogFilter::process(float input)
{
	// process input
	mX = input - mR * y4;

	//Four cascaded onepole filters (bilinear transform)
	y1 = mX * mP + mOldX * mP - mK * y1;
	y2 = y1 * mP + mOldY1 * mP - mK * y2;
	y3 = y2 * mP + mOldY2 * mP - mK * y3;
	y4 = y3 * mP + mOldY3 * mP - mK * y4;

	//Clipper band limited sigmoid
	y4 -= (y4 * y4 * y4) / 6.f;

	mOldX = mX; mOldY1 = y1; mOldY2 = y2; mOldY3 = y3;
	return y4;
}

float MoogFilter::getCutoff() const
{
	return mCutoff;
}

void MoogFilter::setCutoff(float cutoffHz)
{
	mCutoff = cutoffHz; calc();
}

float MoogFilter::getRes() const
{
	return mResonance;
}

void MoogFilter::setRes(float resonance)
{
	mResonance = resonance; calc();
}