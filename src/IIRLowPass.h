class IIRLowPass
{
public:
    IIRLowPass(unsigned int sample_rate) :
        mPrevious(0.f),
        mSampleRate(sample_rate),
        mCutoffFrequency(sample_rate)
    {
        setCutoff(sample_rate);
    }

    void setSampleRate(unsigned int sample_rate)
    {
        mSampleRate = sample_rate;
        setCutoff(mCutoffFrequency);
    }

    void setCutoff(float cutoff_frequency)
    {
        mCutoffFrequency = cutoff_frequency;
        float delta_t = 1.f / mSampleRate;
        float numerator = 2.f * M_PI * delta_t * cutoff_frequency;
        mAlpha = numerator / (numerator + 1.f);
    }

    float process(float sample)
    {
        float out = mPrevious + mAlpha * (sample - mPrevious);
        mPrevious = sample;
        return out;
    }

private:
    float mAlpha;
    float mPrevious;
    unsigned int mSampleRate;
    float mCutoffFrequency;
};