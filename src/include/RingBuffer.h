#pragma once

#include <stdlib.h>
#include <stdint.h>

constexpr bool is_powerof2(int v) {
    return v && ((v & (v - 1)) == 0);
}


/**
 * Efficient ring buffer class that avoids calculating expensive modulo to wraparound.
 * The main limitation is that the buffer size must be the power of two.
*/
template <typename T, size_t max_size>
class RingBuffer
{
    static_assert(is_powerof2(max_size));
  public:
    RingBuffer() {
        mWrapMask = max_size - 1;
        reset(); 
    }

    /** clears buffer, sets write ptr to 0, and delay to 1 sample.
    */
    void reset()
    {
        for(size_t i = 0; i < max_size; i++)
        {
            mBuffer[i] = T(0);
        }
        mWriteIndex = 0;
    }

    /** writes the sample of type T to the delay line, and advances the write ptr
    */
    inline void write(const T sample)
    {
        mBuffer[mWriteIndex++] = sample;
        mWriteIndex &= mWrapMask;
    }

    /** returns the next sample of type T in the delay line, interpolated if necessary.
    */
    inline const T read() const
    {
        return mBuffer[mWriteIndex&mWrapMask];
    }

  private:
    size_t mWriteIndex;
    uint32_t mWrapMask;
    T mBuffer[max_size];
};