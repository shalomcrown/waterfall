#pragma once

#include <QString>

#include <cstdint>
#include <span>

// Writes a mono MP3 file with LAME. The parent directory is created when missing.
class Mp3File {
public:
    static bool write(const QString& path, int sampleRate, std::span<const std::int16_t> samples, QString& error);
};
