#include "FftEngine.h"

#include <pocketfft_hdronly.h>

#include <algorithm>
#include <cmath>
#include <complex>

void FftEngine::configure(int fftSize) {
    if (fftSize < 2) {
        m_size = 0;
        m_coherentGain = 1.f;
        m_window.clear();
        return;
    }
    m_size = fftSize;
    m_window.resize(static_cast<size_t>(fftSize));
    constexpr double kPi = 3.14159265358979323846;
    double gain = 0.0;
    for (int i = 0; i < fftSize; ++i) {
        const double weight = 0.5 * (1.0 - std::cos(2.0 * kPi * i / fftSize));
        m_window[static_cast<size_t>(i)] = static_cast<float>(weight);
        gain += weight;
    }
    m_coherentGain = static_cast<float>(gain / fftSize);
    if (m_coherentGain <= 0.f) {
        m_coherentGain = 1.f;
    }
}

// ================================================================================

bool FftEngine::process(std::span<const float> input, std::span<float> levels) const {
    if (m_size < 2 || static_cast<int>(input.size()) != m_size) {
        return false;
    }
    const int bins = binCount();
    if (static_cast<int>(levels.size()) != bins) {
        return false;
    }

    std::vector<float> windowed(static_cast<size_t>(m_size));
    for (int i = 0; i < m_size; ++i) {
        windowed[static_cast<size_t>(i)] = input[static_cast<size_t>(i)] * m_window[static_cast<size_t>(i)];
    }
    std::vector<std::complex<float>> spectrum(static_cast<size_t>(bins));
    const pocketfft::shape_t shape{static_cast<size_t>(m_size)};
    const pocketfft::stride_t strideIn{static_cast<std::ptrdiff_t>(sizeof(float))};
    const pocketfft::stride_t strideOut{static_cast<std::ptrdiff_t>(sizeof(std::complex<float>))};
    pocketfft::r2c(shape, strideIn, strideOut, 0, pocketfft::FORWARD, windowed.data(), spectrum.data(), 1.f, 1);

    const float n = static_cast<float>(m_size);
    for (int bin = 0; bin < bins; ++bin) {
        const bool edge = bin == 0 || bin == bins - 1;
        const float scale = (edge ? 1.f : 2.f) / n;
        const float amplitude = std::abs(spectrum[static_cast<size_t>(bin)]) * scale / m_coherentGain;
        const float safe = std::max(amplitude, 1e-10f);
        const float db = 20.f * std::log10(safe);
        const float level = (db - kDbFloor) / (0.f - kDbFloor);
        levels[static_cast<size_t>(bin)] = std::clamp(level, 0.f, 1.f);
    }
    return true;
}
