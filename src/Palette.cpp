#include "Palette.h"

#include <algorithm>
#include <cmath>

namespace {
struct Stop {
    float t;
    int r;
    int g;
    int b;
};

QRgb sample(const Stop* stops, int count, float level) {
    if (std::isfinite(level) == false) {
        level = 0.f;
    }
    level = std::clamp(level, 0.f, 1.f);
    if (level <= stops[0].t) {
        return qRgb(stops[0].r, stops[0].g, stops[0].b);
    }
    for (int i = 1; i < count; ++i) {
        if (level > stops[i].t) {
            continue;
        }
        const float span = stops[i].t - stops[i - 1].t;
        const float u = span <= 0.f ? 0.f : (level - stops[i - 1].t) / span;
        const auto mix = [u](int from, int to) {
            return static_cast<int>(std::lround(from + (to - from) * u));
        };
        return qRgb(mix(stops[i - 1].r, stops[i].r), mix(stops[i - 1].g, stops[i].g),
                    mix(stops[i - 1].b, stops[i].b));
    }
    const Stop& last = stops[count - 1];
    return qRgb(last.r, last.g, last.b);
}

// ================================================================================

QRgb gray(float level) {
    const Stop stops[] = {{0.f, 0, 0, 0}, {1.f, 255, 255, 255}};
    return sample(stops, 2, level);
}

// ================================================================================

QRgb viridis(float level) {
    const Stop stops[] = {{0.f, 68, 1, 84},  {0.25f, 59, 82, 139},   {0.5f, 33, 145, 140},
                           {0.75f, 94, 201, 98}, {1.f, 253, 231, 37}};
    return sample(stops, 5, level);
}

// ================================================================================

QRgb magma(float level) {
    const Stop stops[] = {{0.f, 0, 0, 4},     {0.25f, 59, 15, 112},  {0.5f, 140, 41, 129},
                           {0.75f, 222, 73, 104}, {1.f, 252, 253, 191}};
    return sample(stops, 5, level);
}

// ================================================================================

QRgb inferno(float level) {
    const Stop stops[] = {{0.f, 0, 0, 4},      {0.25f, 66, 10, 104}, {0.5f, 187, 55, 84},
                           {0.75f, 249, 142, 9}, {1.f, 252, 255, 164}};
    return sample(stops, 5, level);
}

// ================================================================================

QRgb plasma(float level) {
    const Stop stops[] = {{0.f, 13, 8, 135},   {0.25f, 126, 3, 168}, {0.5f, 204, 71, 120},
                           {0.75f, 248, 149, 64}, {1.f, 240, 249, 33}};
    return sample(stops, 5, level);
}

// ================================================================================

QRgb turbo(float level) {
    const Stop stops[] = {{0.f, 48, 18, 59},    {0.25f, 33, 145, 140}, {0.5f, 122, 209, 81},
                           {0.75f, 237, 167, 47}, {1.f, 122, 4, 3}};
    return sample(stops, 5, level);
}

// ================================================================================

QRgb jet(float level) {
    const Stop stops[] = {{0.f, 0, 0, 127},     {0.11f, 0, 0, 255},    {0.36f, 0, 255, 255},
                           {0.625f, 255, 255, 0}, {0.89f, 255, 0, 0},   {1.f, 127, 0, 0}};
    return sample(stops, 6, level);
}

// ================================================================================

QRgb hot(float level) {
    const Stop stops[] = {{0.f, 0, 0, 0}, {0.33f, 255, 0, 0}, {0.66f, 255, 255, 0}, {1.f, 255, 255, 255}};
    return sample(stops, 4, level);
}

// ================================================================================

QRgb cool(float level) {
    const Stop stops[] = {{0.f, 0, 255, 255}, {1.f, 255, 0, 255}};
    return sample(stops, 2, level);
}
}  // namespace

// ================================================================================

QRgb Palette::color(const QString& name, float level) {
    if (name == QStringLiteral("Gray")) {
        return gray(level);
    }
    if (name == QStringLiteral("Magma")) {
        return magma(level);
    }
    if (name == QStringLiteral("Inferno")) {
        return inferno(level);
    }
    if (name == QStringLiteral("Plasma")) {
        return plasma(level);
    }
    if (name == QStringLiteral("Turbo")) {
        return turbo(level);
    }
    if (name == QStringLiteral("Jet")) {
        return jet(level);
    }
    if (name == QStringLiteral("Hot")) {
        return hot(level);
    }
    if (name == QStringLiteral("Cool")) {
        return cool(level);
    }
    return viridis(level);
}
