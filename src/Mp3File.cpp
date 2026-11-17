#include "Mp3File.h"

#include <lame.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <algorithm>
#include <vector>

namespace {
constexpr int kBitrateKbps = 128;
constexpr int kChunkSamples = 1152 * 32;

struct LameSession {
    lame_global_flags* flags = nullptr;

    ~LameSession() {
        if (flags != nullptr) {
            lame_close(flags);
        }
    }
};

// ================================================================================

int mp3BufferBytes(int samples) {
    return static_cast<int>(1.25 * static_cast<double>(samples)) + 7200;
}

// ================================================================================

bool mp3RateSupported(int sampleRate) {
    switch (sampleRate) {
    case 8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        return true;
    default:
        return false;
    }
}
}  // namespace

// ================================================================================

bool Mp3File::write(const QString& path, int sampleRate, std::span<const std::int16_t> samples, QString& error) {
    error.clear();
    if (path.isEmpty() == true || sampleRate < 1) {
        error = QObject::tr("The recording has no sample rate.");
        return false;
    }
    if (mp3RateSupported(sampleRate) == false) {
        error = QObject::tr("MP3 cannot be saved at %1 Hz. Save as WAV instead.").arg(sampleRate);
        return false;
    }
    const QFileInfo info(path);
    if (info.absolutePath().isEmpty() == false && QDir().mkpath(info.absolutePath()) == false) {
        error = QObject::tr("The folder could not be created.");
        return false;
    }

    LameSession session;
    session.flags = lame_init();
    if (session.flags == nullptr) {
        error = QObject::tr("The MP3 encoder could not start.");
        return false;
    }
    lame_set_num_channels(session.flags, 1);
    lame_set_mode(session.flags, MONO);
    lame_set_in_samplerate(session.flags, sampleRate);
    lame_set_brate(session.flags, kBitrateKbps);
    lame_set_bWriteVbrTag(session.flags, 0);
    if (lame_init_params(session.flags) < 0) {
        error = QObject::tr("MP3 cannot be saved at %1 Hz. Save as WAV instead.").arg(sampleRate);
        return false;
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate) == false) {
        error = file.errorString();
        return false;
    }

    const int capacity = mp3BufferBytes(kChunkSamples);
    std::vector<unsigned char> encoded(static_cast<size_t>(capacity));
    size_t offset = 0;
    while (offset < samples.size()) {
        const int count = static_cast<int>(std::min(samples.size() - offset, static_cast<size_t>(kChunkSamples)));
        const int written = lame_encode_buffer(session.flags, samples.data() + offset, nullptr, count, encoded.data(),
                                               capacity);
        if (written < 0) {
            error = QObject::tr("The recording could not be saved as MP3.");
            return false;
        }
        if (written > 0 && file.write(reinterpret_cast<const char*>(encoded.data()), written) != written) {
            error = file.errorString();
            return false;
        }
        offset += static_cast<size_t>(count);
    }
    const int tail = lame_encode_flush(session.flags, encoded.data(), capacity);
    if (tail < 0) {
        error = QObject::tr("The recording could not be saved as MP3.");
        return false;
    }
    if (tail > 0 && file.write(reinterpret_cast<const char*>(encoded.data()), tail) != tail) {
        error = file.errorString();
        return false;
    }
    if (file.error() != QFile::NoError) {
        error = file.errorString();
        return false;
    }
    return true;
}
