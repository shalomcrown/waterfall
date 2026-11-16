#pragma once

#include <span>
#include <vector>

// Real FFT with a periodic Hann window. Magnitudes are scaled back by the
// window's coherent gain, converted to dB, and mapped from [-100, 0] onto
// [0, 1]. One-sided bins: index 0 is DC, the last index is Nyquist.
class FftEngine {
public:
    static constexpr float kDbFloor = -100.f;

    void configure(int fftSize);
    int size() const { return m_size; }
    int binCount() const { return m_size / 2 + 1; }

    bool process(std::span<const float> input, std::span<float> levels) const;

private:
    int m_size = 0;
    float m_coherentGain = 1.f;
    std::vector<float> m_window;
};
