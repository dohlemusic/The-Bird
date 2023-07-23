/**
* Second order Butterworth low-pass filter
* Dimitris Tassopoulos 2016-2020
*
* fc, corner frequency
* Butterworth low-pass and high-pass filters are specialized versions of the ordinary secondorder
* low-pass filter. Their Q values are fixed at 0.707, which is the largest value it can
* assume before peaking in the frequency response is observed.
*/
#pragma once
#include "filters/Biquad.h"
#include <complex>
#include <cmath>

namespace DSPFilters
{
    class Butterworth : public Biquad {
    public:
        Butterworth(float cutoff, float sampleRate)
        {
            m_sampleRate = sampleRate;
            setCutoff(cutoff);
        }

        void setCutoff(float frequency)
        {
            float c = 1.f / (tanf((float)pi * frequency / m_sampleRate));
            m_coeffs.a0 = 1.f / (1.f + (float)sqrt2 * c + powf(c, 2.f));
            m_coeffs.a1 = 2.f * m_coeffs.a0;
            m_coeffs.a2 = m_coeffs.a0;
            m_coeffs.b1 = 2.f * m_coeffs.a0 * (1.f - powf(c, 2.f));
            m_coeffs.b2 = m_coeffs.a0 * (1.f - (float)sqrt2 * c + powf(c, 2.f));
        }

    private:
        float m_sampleRate;
    };


}