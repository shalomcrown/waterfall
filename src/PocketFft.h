#pragma once

#include <complex>

// One-sided real FFT. output has size / 2 + 1 bins. This file is compiled as
// C++14 so PocketFFT uses malloc, which MinGW's C runtime can free.
void realForwardFft(const float* input, std::complex<float>* output, int size);
