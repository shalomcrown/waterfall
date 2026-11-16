#pragma once

#include <QRgb>
#include <QString>

// Maps a normalized level in [0, 1] through one of the named heatmaps.
// An unknown name uses Viridis.
class Palette {
public:
    static QRgb color(const QString& name, float level);
};
