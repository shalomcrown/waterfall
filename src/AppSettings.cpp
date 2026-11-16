#include "AppSettings.h"

#include <QSettings>

namespace {
QSettings store() {
    return QSettings();
}
}  // namespace

// ================================================================================

QString AppSettings::inputDeviceId() {
    return store().value(QStringLiteral("audio/inputDeviceId")).toString();
}

// ================================================================================

void AppSettings::setInputDeviceId(const QString& id) {
    store().setValue(QStringLiteral("audio/inputDeviceId"), id);
}

// ================================================================================

QString AppSettings::outputDeviceId() {
    return store().value(QStringLiteral("audio/outputDeviceId")).toString();
}

// ================================================================================

void AppSettings::setOutputDeviceId(const QString& id) {
    store().setValue(QStringLiteral("audio/outputDeviceId"), id);
}

// ================================================================================

int AppSettings::sampleRate() {
    return store().value(QStringLiteral("audio/sampleRate"), 48000).toInt();
}

// ================================================================================

void AppSettings::setSampleRate(int rate) {
    store().setValue(QStringLiteral("audio/sampleRate"), rate);
}

// ================================================================================

int AppSettings::fftSize() {
    return store().value(QStringLiteral("audio/fftSize"), 2048).toInt();
}

// ================================================================================

void AppSettings::setFftSize(int size) {
    store().setValue(QStringLiteral("audio/fftSize"), size);
}

// ================================================================================

int AppSettings::overlapPercent() {
    return store().value(QStringLiteral("audio/overlapPercent"), 50).toInt();
}

// ================================================================================

void AppSettings::setOverlapPercent(int percent) {
    store().setValue(QStringLiteral("audio/overlapPercent"), percent);
}

// ================================================================================

int AppSettings::maxFrequencyHz() {
    return store().value(QStringLiteral("audio/maxFrequencyHz"), 8000).toInt();
}

// ================================================================================

void AppSettings::setMaxFrequencyHz(int hz) {
    store().setValue(QStringLiteral("audio/maxFrequencyHz"), hz);
}

// ================================================================================

bool AppSettings::keepScreenOn() {
    return store().value(QStringLiteral("display/keepScreenOn"), false).toBool();
}

// ================================================================================

void AppSettings::setKeepScreenOn(bool keep) {
    store().setValue(QStringLiteral("display/keepScreenOn"), keep);
}

// ================================================================================

QString AppSettings::palette() {
    return store().value(QStringLiteral("display/palette"), QStringLiteral("Viridis")).toString();
}

// ================================================================================

void AppSettings::setPalette(const QString& name) {
    store().setValue(QStringLiteral("display/palette"), name);
}

// ================================================================================

QList<int> AppSettings::sampleRates() {
    return {8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 192000};
}

// ================================================================================

QList<int> AppSettings::fftSizes() {
    return {1024, 2048, 4096};
}

// ================================================================================

QList<int> AppSettings::overlapPercents() {
    return {0, 25, 50, 75, 87};
}

// ================================================================================

QList<int> AppSettings::maxFrequenciesHz() {
    return {4000, 8000, 12000, 16000, 24000, 0};
}

// ================================================================================

QStringList AppSettings::paletteNames() {
    return {QStringLiteral("Gray"),    QStringLiteral("Viridis"), QStringLiteral("Magma"),
            QStringLiteral("Inferno"), QStringLiteral("Plasma"),  QStringLiteral("Turbo"),
            QStringLiteral("Jet"),     QStringLiteral("Hot"),     QStringLiteral("Cool")};
}
