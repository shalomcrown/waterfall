#pragma once

#include <QRgb>
#include <QString>
#include <QVector>

#include <span>

// One color column from one FFT result. Row 0 is the top of the waterfall: the
// highest displayed frequency. The live path and the redraw path both call this,
// so a column looks the same whichever one made it.
struct SpectrumColumnSettings {
    int fftSize = 0;
    int sampleRate = 0;
    int maxFrequencyHz = 0;
    bool logarithmic = false;
    int logFloorHz = 5;
    QString palette;
    // When positive and below the number of bins shown, the column has this many rows
    // and each row shows the strongest bin it covers, so a narrow peak is not lost
    // when the picture has fewer pixels than the FFT has bins.
    int maxRows = 0;
};

void buildSpectrumColumn(std::span<const float> levels, const SpectrumColumnSettings& settings,
                         QVector<QRgb>& column);
