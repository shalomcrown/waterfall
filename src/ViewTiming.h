#pragma once

#include <algorithm>
#include <cmath>

// Time zoom is a ladder of levels. Level 0 is the capture hop. Each step is a
// factor of the square root of two: a positive level zooms out (a larger hop
// between columns) and a negative level zooms in. Zooming in stops adding columns
// at the hop of the maximum overlap, and further steps draw each column wider.
struct LevelGeometry {
    int hop = 1;
    int pixelsPerColumn = 1;
};

constexpr int kMaxZoomOutLevel = 12;
constexpr int kMaxPixelsPerColumn = 16;
// The narrowest part of the frequency axis the view can zoom to, as a fraction.
constexpr double kMinFrequencySpan = 0.02;

// ================================================================================

inline int minimumHop(int fftSize, int baseHop) {
    return std::max(1, std::min(baseHop, fftSize / 8));
}

// ================================================================================

inline LevelGeometry levelGeometry(int level, int fftSize, int baseHop) {
    LevelGeometry geometry;
    const int base = std::max(1, baseHop);
    const int floorHop = minimumHop(fftSize, base);
    const double exact = static_cast<double>(base) * std::pow(2.0, static_cast<double>(level) / 2.0);
    if (exact >= static_cast<double>(floorHop)) {
        geometry.hop = std::max(floorHop, static_cast<int>(std::lround(exact)));
        geometry.pixelsPerColumn = 1;
        return geometry;
    }
    geometry.hop = floorHop;
    geometry.pixelsPerColumn =
            std::clamp(static_cast<int>(std::lround(static_cast<double>(floorHop) / exact)), 1, kMaxPixelsPerColumn);
    return geometry;
}

// ================================================================================

// The most zoomed-in level: columns are as wide as they are allowed to get.
inline int minimumZoomLevel(int fftSize, int baseHop) {
    int level = 0;
    while (level > -60 && levelGeometry(level, fftSize, baseHop).pixelsPerColumn < kMaxPixelsPerColumn) {
        --level;
    }
    return level;
}

// ================================================================================

// The level whose zoom is nearest to the current level scaled by factor. A factor
// above one zooms in.
inline int levelAfterScale(int level, double factor, int fftSize, int baseHop) {
    if (factor <= 0.0) {
        return level;
    }
    const int steps = static_cast<int>(std::lround(2.0 * std::log2(factor)));
    return std::clamp(level - steps, minimumZoomLevel(fftSize, baseHop), kMaxZoomOutLevel);
}

// ================================================================================

// The window end of grid column k: the grid is fixed to absolute sample positions,
// so a column keeps the same audio however the view is moved.
inline long long gridEnd(long long column, int fftSize, int hop) {
    return static_cast<long long>(fftSize) + column * static_cast<long long>(hop);
}

// ================================================================================

// The grid column at or before position, or -1 when none fits.
inline long long gridColumnAtOrBefore(long long position, int fftSize, int hop) {
    if (position < fftSize || hop < 1) {
        return -1;
    }
    return (position - fftSize) / hop;
}

// ================================================================================

// The longest FFT the redraw uses. This is also the largest FFT size in Settings.
constexpr int kMaxRenderFftSize = 32768;

// Zooming in on frequency makes the FFT longer, so the bins get narrower as fast as
// the picture is magnified: each halving of the shown span doubles the window. The
// capture FFT is the shortest it goes. The column spacing does not change, so the
// overlap rises on its own.
inline int effectiveFftSize(int baseFftSize, double span) {
    if (baseFftSize < 2 || span >= 0.999) {
        return baseFftSize;
    }
    const double steps = std::round(std::log2(1.0 / std::max(span, 1e-6)));
    int size = baseFftSize;
    for (int i = 0; i < static_cast<int>(steps) && size * 2 <= kMaxRenderFftSize; ++i) {
        size *= 2;
    }
    return size;
}

// ================================================================================

// How much each window overlaps the one before it, in percent.
inline double overlapPercentOf(int hop, int fftSize) {
    if (fftSize < 1) {
        return 0.0;
    }
    return std::clamp(100.0 * (1.0 - static_cast<double>(hop) / static_cast<double>(fftSize)), 0.0, 100.0);
}
