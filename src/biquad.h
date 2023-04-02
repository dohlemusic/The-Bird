/**
* First order all-pass filter
* Dimitris Tassopoulos 2016
*
* fc, corner frequency
*/
#pragma once
#include "filter_common.h"

namespace DSPFilters
{
	class Biquad {
	public:
		Biquad() : m_xnz1(0), m_xnz2(0), m_ynz1(0), m_ynz2(0), m_offset(0), m_coeffs{ 0 } {};
		virtual ~Biquad() {};
		float update(float sample)
		{
			float xn = sample;
			float yn = m_coeffs.a0 * xn + m_coeffs.a1 * m_xnz1 + m_coeffs.a2 * m_xnz2
				- m_coeffs.b1 * m_ynz1 - m_coeffs.b2 * m_ynz2;
			m_xnz2 = m_xnz1;
			m_xnz1 = xn;
			m_ynz2 = m_ynz1;
			m_ynz1 = yn;
			return(yn + m_offset);
		}

		void set_offset(float offset)
		{
			m_offset = offset;
		}

		float get_offset(void)
		{
			return(m_offset);
		}

		typedef struct {
			float a0;
			float a1;
			float a2;
			float b1;
			float b2;
			float c0;
			float d0;
		} tp_coeffs;

	protected:
		float m_xnz1, m_xnz2, m_ynz1, m_ynz2, m_offset;
		tp_coeffs m_coeffs;
	};

}