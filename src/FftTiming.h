#pragma once

#include <algorithm>

// Shared by the pipeline and the axes. 87 means 87.5% overlap.
inline int waterfallHop(int fftSize, int overlapPercent) {
    if (fftSize < 2) {
        return 0;
    }
    const int percent = std::clamp(overlapPercent, 0, 87);
    if (percent >= 87) {
        return std::max(1, fftSize / 8);
    }
    const int hop = fftSize - (fftSize * percent) / 100;
    return std::clamp(hop, 1, fftSize);
}

// ================================================================================

// A non-positive limit means the full range up to Nyquist.
inline double displayedMaxHz(int sampleRate, int maxFrequencyHz) {
    if (sampleRate < 2) {
        return maxFrequencyHz > 0 ? static_cast<double>(maxFrequencyHz) : 0.0;
    }
    const double nyquist = static_cast<double>(sampleRate) / 2.0;
    if (maxFrequencyHz <= 0 || static_cast<double>(maxFrequencyHz) >= nyquist) {
        return nyquist;
    }
    return static_cast<double>(maxFrequencyHz);
}
