#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Cross-platform store (registry, plist, or ini) via QSettings.
class AppSettings {
public:
    static QString inputDeviceId();
    static void setInputDeviceId(const QString& id);

    static QString outputDeviceId();
    static void setOutputDeviceId(const QString& id);

    static int sampleRate();
    static void setSampleRate(int rate);

    static int fftSize();
    static void setFftSize(int size);

    // Percent of the FFT window kept for the next column. 87 means 87.5%.
    static int overlapPercent();
    static void setOverlapPercent(int percent);

    static QString palette();
    static void setPalette(const QString& name);

    static QList<int> sampleRates();
    static QList<int> fftSizes();
    static QList<int> overlapPercents();
    static QStringList paletteNames();
};
