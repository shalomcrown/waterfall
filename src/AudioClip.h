#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <vector>

// A mono 16-bit clip decoded from a WAV or MP3 file.
struct AudioClip {
    std::vector<std::int16_t> samples;
    int sampleRate = 0;

    static bool load(const QString& path, AudioClip& clip, QString& error);
    static bool decode(const QByteArray& bytes, AudioClip& clip, QString& error);
};
