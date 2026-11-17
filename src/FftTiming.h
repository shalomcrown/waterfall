#pragma once

#include <algorithm>
#include <cmath>

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

// ================================================================================

// A logarithmic scale starts at this floor. It has to stay below the top.
inline double clampedLogFloorHz(double maximumHz, double floorHz) {
    const double requested = floorHz > 0.0 ? floorHz : 5.0;
    if (maximumHz <= requested) {
        return maximumHz > 1.0 ? 1.0 : maximumHz * 0.5;
    }
    return requested;
}

// ================================================================================

// 0 is the bottom of the axis. 1 is the top.
inline double frequencyFromBottom(double hz, double maximumHz, bool logarithmic, double floorHz) {
    if (maximumHz <= 0.0) {
        return 0.0;
    }
    if (logarithmic == false) {
        return std::clamp(hz / maximumHz, 0.0, 1.0);
    }
    const double floor = clampedLogFloorHz(maximumHz, floorHz);
    if (maximumHz <= floor) {
        return std::clamp(hz / maximumHz, 0.0, 1.0);
    }
    const double clamped = std::clamp(hz, floor, maximumHz);
    return std::log(clamped / floor) / std::log(maximumHz / floor);
}

// ================================================================================

inline double hzFromBottom(double fractionFromBottom, double maximumHz, bool logarithmic, double floorHz) {
    const double fraction = std::clamp(fractionFromBottom, 0.0, 1.0);
    if (logarithmic == false || maximumHz <= 0.0) {
        return fraction * maximumHz;
    }
    const double floor = clampedLogFloorHz(maximumHz, floorHz);
    if (maximumHz <= floor) {
        return fraction * maximumHz;
    }
    return floor * std::pow(maximumHz / floor, fraction);
}
