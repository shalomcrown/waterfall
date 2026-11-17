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

    // Hertz. 0 means the full range up to Nyquist.
    static int maxFrequencyHz();
    static void setMaxFrequencyHz(int hz);

    // Logarithmic frequency scale. Linear is the default.
    static bool logarithmicFrequency();
    static void setLogarithmicFrequency(bool logarithmic);

    // Lowest frequency drawn when the scale is logarithmic. Default 5 Hz.
    static int logFloorHz();
    static void setLogFloorHz(int hz);

    // How many seconds of recent audio Save keeps. Default 5 minutes.
    static int recordingSeconds();
    static void setRecordingSeconds(int seconds);

    // "wav", or another id from RecordingSave::formats(). Default wav.
    static QString recordingFormat();
    static void setRecordingFormat(const QString& id);

    static bool keepScreenOn();
    static void setKeepScreenOn(bool keep);

    static QString palette();
    static void setPalette(const QString& name);

    static QList<int> sampleRates();
    static QList<int> fftSizes();
    static QList<int> overlapPercents();
    static QList<int> maxFrequenciesHz();
    static QList<int> logFloorChoices();
    static QList<int> recordingSecondsChoices();
    static QStringList paletteNames();
};
