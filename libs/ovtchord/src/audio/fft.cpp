// fft.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "audio/fft.h"
#include <cmath>

namespace ovtchord
{
    FFT::FFT (int log2Size) : log2Size_ (log2Size), size_ (1 << log2Size) {}

    void FFT::bitReverse (std::vector<std::complex<float>>& data)
    {
        for (int i = 1, j = 0; i < size_; ++i)
        {
            int bit = size_ >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap (data[i], data[j]);
        }
    }

    void FFT::forward (std::vector<std::complex<float>>& data)
    {
        bitReverse (data);
        for (int len = 2; len <= size_; len <<= 1)
        {
            const float angle = -2.0f * static_cast<float> (std::acos (-1.0)) / static_cast<float> (len);
            const std::complex<float> wlen (std::cos (angle), std::sin (angle));
            for (int i = 0; i < size_; i += len)
            {
                std::complex<float> w (1.0f, 0.0f);
                const int half = len / 2;
                for (int j = 0; j < half; ++j)
                {
                    const std::complex<float> u = data[i + j];
                    const std::complex<float> v = data[i + j + half] * w;
                    data[i + j]       = u + v;
                    data[i + j + half] = u - v;
                    w *= wlen;
                }
            }
        }
    }
}
