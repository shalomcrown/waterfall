#include "WavFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace {
void write16(QFile& file, quint16 value) {
    const char bytes[2] = {static_cast<char>(value & 0xff), static_cast<char>((value >> 8) & 0xff)};
    file.write(bytes, 2);
}

// ================================================================================

void write32(QFile& file, quint32 value) {
    const char bytes[4] = {static_cast<char>(value & 0xff), static_cast<char>((value >> 8) & 0xff),
                           static_cast<char>((value >> 16) & 0xff), static_cast<char>((value >> 24) & 0xff)};
    file.write(bytes, 4);
}
}  // namespace

// ================================================================================

bool WavFile::write(const QString& path, int sampleRate, std::span<const std::int16_t> samples, QString& error) {
    error.clear();
    if (path.isEmpty() == true || sampleRate < 1) {
        error = QStringLiteral("The recording has no sample rate.");
        return false;
    }
    const QFileInfo info(path);
    if (info.absolutePath().isEmpty() == false && QDir().mkpath(info.absolutePath()) == false) {
        error = QStringLiteral("The folder could not be created.");
        return false;
    }
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate) == false) {
        error = file.errorString();
        return false;
    }
    const auto dataBytes = static_cast<quint32>(samples.size() * sizeof(std::int16_t));
    file.write("RIFF", 4);
    write32(file, 36 + dataBytes);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    write32(file, 16);
    write16(file, 1);
    write16(file, 1);
    write32(file, static_cast<quint32>(sampleRate));
    write32(file, static_cast<quint32>(sampleRate) * 2);
    write16(file, 2);
    write16(file, 16);
    file.write("data", 4);
    write32(file, dataBytes);
    constexpr int kChunkSamples = 32768;
    char chunk[kChunkSamples * 2];
    size_t offset = 0;
    while (offset < samples.size()) {
        const size_t count = std::min(samples.size() - offset, static_cast<size_t>(kChunkSamples));
        char* out = chunk;
        for (size_t index = 0; index < count; ++index) {
            const auto value = static_cast<quint16>(samples[offset + index]);
            *out++ = static_cast<char>(value & 0xff);
            *out++ = static_cast<char>((value >> 8) & 0xff);
        }
        if (file.write(chunk, static_cast<qint64>(count * 2)) != static_cast<qint64>(count * 2)) {
            error = file.errorString();
            return false;
        }
        offset += count;
    }
    if (file.error() != QFile::NoError) {
        error = file.errorString();
        return false;
    }
    return true;
}
