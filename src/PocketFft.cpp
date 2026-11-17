#include "PocketFft.h"

#include <pocketfft_hdronly.h>

#include <cstddef>

void realForwardFft(const float* input, std::complex<float>* output, int size) {
    const pocketfft::shape_t shape{static_cast<size_t>(size)};
    const pocketfft::stride_t strideIn{static_cast<std::ptrdiff_t>(sizeof(float))};
    const pocketfft::stride_t strideOut{static_cast<std::ptrdiff_t>(sizeof(std::complex<float>))};
    pocketfft::r2c(shape, strideIn, strideOut, 0, pocketfft::FORWARD, input, output, 1.f, 1);
}
