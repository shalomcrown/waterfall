#pragma once

#include <QString>

#include <cstdint>
#include <span>

// Writes a mono 16-bit PCM WAV file. The parent directory is created when missing.
class WavFile {
public:
    static bool write(const QString& path, int sampleRate, std::span<const std::int16_t> samples, QString& error);
};
