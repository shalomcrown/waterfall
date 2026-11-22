#include "SpectrumColumn.h"

#include "FftTiming.h"
#include "Palette.h"

#include <algorithm>
#include <cmath>

// ================================================================================

void buildSpectrumColumn(std::span<const float> levels, const SpectrumColumnSettings& settings,
                         QVector<QRgb>& column) {
    const int bins = static_cast<int>(levels.size());
    if (bins < 2 || settings.fftSize < 2) {
        column.clear();
        return;
    }
    int lastBin = bins - 1;
    if (settings.sampleRate > 1 && settings.maxFrequencyHz > 0 &&
        static_cast<double>(settings.maxFrequencyHz) < static_cast<double>(settings.sampleRate) / 2.0) {
        const int limited = static_cast<int>(std::lround(static_cast<double>(settings.maxFrequencyHz) *
                                                         settings.fftSize / static_cast<double>(settings.sampleRate)));
        lastBin = std::clamp(limited, 1, bins - 1);
    }
    const int shown = lastBin + 1;
    const int rows = settings.maxRows >= 2 && settings.maxRows < shown ? settings.maxRows : shown;
    column.resize(rows);
    const double topHz = settings.sampleRate > 1 ? static_cast<double>(lastBin) *
                                                           static_cast<double>(settings.sampleRate) /
                                                           static_cast<double>(settings.fftSize)
                                                 : 0.0;
    const int floorHz = settings.logFloorHz > 0 ? settings.logFloorHz : 5;
    const bool logarithmic = settings.logarithmic == true && topHz > static_cast<double>(floorHz);
    if (logarithmic == true) {
        const auto levelAt = [&](double hz) {
            const double bin = hz * static_cast<double>(settings.fftSize) / static_cast<double>(settings.sampleRate);
            if (bin <= 0.0) {
                return levels.front();
            }
            if (bin >= static_cast<double>(bins - 1)) {
                return levels.back();
            }
            const int lower = static_cast<int>(std::floor(bin));
            const float fraction = static_cast<float>(bin - static_cast<double>(lower));
            return levels[static_cast<size_t>(lower)] * (1.f - fraction) +
                   levels[static_cast<size_t>(lower + 1)] * fraction;
        };
        if (rows < shown) {
            for (int i = 0; i < rows; ++i) {
                const int fromBottomRow = rows - 1 - i;
                const double hz0 = hzFromBottom(static_cast<double>(fromBottomRow) / static_cast<double>(rows), topHz,
                                                true, static_cast<double>(floorHz));
                const double hz1 = hzFromBottom(static_cast<double>(fromBottomRow + 1) / static_cast<double>(rows),
                                                topHz, true, static_cast<double>(floorHz));
                const double binsPerHz = static_cast<double>(settings.fftSize) / static_cast<double>(settings.sampleRate);
                const int first = static_cast<int>(std::ceil(hz0 * binsPerHz));
                const int last = std::min(static_cast<int>(std::floor(hz1 * binsPerHz)), bins - 1);
                float level = 0.f;
                if (first > last) {
                    // The row is narrower than one bin: read between the bins.
                    level = levelAt((hz0 + hz1) / 2.0);
                } else {
                    level = levels[static_cast<size_t>(first)];
                    for (int bin = first + 1; bin <= last; ++bin) {
                        level = std::max(level, levels[static_cast<size_t>(bin)]);
                    }
                }
                column[i] = Palette::color(settings.palette, level);
            }
            return;
        }
        for (int i = 0; i < shown; ++i) {
            const double fromBottom = shown == 1 ? 1.0 : 1.0 - static_cast<double>(i) / static_cast<double>(shown - 1);
            const double hz = hzFromBottom(fromBottom, topHz, true, static_cast<double>(floorHz));
            column[i] = Palette::color(settings.palette, levelAt(hz));
        }
        return;
    }
    if (rows < shown) {
        for (int i = 0; i < rows; ++i) {
            const long long fromBottomRow = rows - 1 - i;
            const int first = static_cast<int>(fromBottomRow * shown / rows);
            const int last = std::max(first + 1, static_cast<int>((fromBottomRow + 1) * shown / rows));
            float level = levels[static_cast<size_t>(first)];
            for (int bin = first + 1; bin < last; ++bin) {
                level = std::max(level, levels[static_cast<size_t>(bin)]);
            }
            column[i] = Palette::color(settings.palette, level);
        }
        return;
    }
    for (int i = 0; i < shown; ++i) {
        // Row 0 is the top of the waterfall: the highest displayed frequency.
        const int source = lastBin - i;
        column[i] = Palette::color(settings.palette, levels[static_cast<size_t>(source)]);
    }
}
