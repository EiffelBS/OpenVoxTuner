// fft.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Internal module: minimal radix-2 complex FFT. Not part of the public API.

#pragma once

#include <complex>
#include <vector>

namespace ovtchord
{
    // In-place iterative radix-2 FFT. Size must be a power of two.
    class FFT
    {
    public:
        explicit FFT (int log2Size);

        // Forward transform (in-place).
        void forward (std::vector<std::complex<float>>& data);

        int size() const { return size_; }

    private:
        void bitReverse (std::vector<std::complex<float>>& data);

        int log2Size_;
        int size_;
    };
}
