#pragma once

#include <QString>

#include <cstdint>
#include <span>

// Reads a pending Java exception, if there is one, and clears it.
// Returns false when the JNI call succeeded.
bool androidTakeJavaException(QString& error, const QString& fallback);

bool androidEncoderAvailable(const QString& mimeType);
int androidSdkInt();

// Encodes mono 16-bit PCM into path. formatId is "m4a" or "ogg".
bool androidEncodeFile(const QString& path, const QString& formatId, int sampleRate,
                       std::span<const std::int16_t> samples, QString& error);
