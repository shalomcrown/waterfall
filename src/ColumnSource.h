#pragma once

#include <QRgb>
#include <QVector>

// Where the display gets columns it does not already have. Positions are
// absolute sample positions counted from the first sample of the session. A
// column's position is where its FFT window ends.
class ColumnSource {
public:
    virtual ~ColumnSource() = default;

    // Runs an FFT of fftSize samples over the window that ends at windowEnd. A longer
    // window than the capture FFT gives finer frequency detail. Returns false when
    // that part of the audio is not available. A positive maxRows limits the height of
    // the column: rows then keep the strongest bin they cover.
    virtual bool renderColumn(long long windowEnd, int fftSize, int maxRows, QVector<QRgb>& column) const = 0;
    // The oldest sample still kept, and one past the newest.
    virtual long long firstPosition() const = 0;
    virtual long long endPosition() const = 0;
    // One past the newest sample heard so far: the live edge. For a file that is
    // being played this is the play position, which can be short of endPosition().
    virtual long long livePosition() const = 0;
};
