#include "AudioClip.h"

#include <lame.h>

#include <QFile>
#include <QObject>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {
constexpr qint64 kMaxFileBytes = 256LL * 1024LL * 1024LL;

struct Reader {
    const QByteArray& bytes;
    int offset = 0;

    bool take(int count, const char*& out) {
        if (count < 0 || offset + count > bytes.size()) {
            return false;
        }
        out = bytes.constData() + offset;
        offset += count;
        return true;
    }

    bool u16(quint16& value) {
        const char* raw = nullptr;
        if (take(2, raw) == false) {
            return false;
        }
        value = static_cast<quint16>(static_cast<unsigned char>(raw[0]) |
                                     (static_cast<unsigned char>(raw[1]) << 8));
        return true;
    }

    bool u32(quint32& value) {
        const char* raw = nullptr;
        if (take(4, raw) == false) {
            return false;
        }
        value = static_cast<quint32>(static_cast<unsigned char>(raw[0])) |
                (static_cast<quint32>(static_cast<unsigned char>(raw[1])) << 8) |
                (static_cast<quint32>(static_cast<unsigned char>(raw[2])) << 16) |
                (static_cast<quint32>(static_cast<unsigned char>(raw[3])) << 24);
        return true;
    }
};

// ================================================================================

bool looksLikeWav(const QByteArray& bytes) {
    return bytes.size() >= 12 && std::memcmp(bytes.constData(), "RIFF", 4) == 0 &&
           std::memcmp(bytes.constData() + 8, "WAVE", 4) == 0;
}

// ================================================================================

bool decodeWav(const QByteArray& bytes, AudioClip& clip, QString& error) {
    Reader reader{bytes, 12};
    quint16 audioFormat = 0;
    quint16 channels = 0;
    quint32 sampleRate = 0;
    quint16 bits = 0;
    bool haveFormat = false;
    const char* data = nullptr;
    quint32 dataBytes = 0;
    while (reader.offset + 8 <= bytes.size()) {
        const char* id = nullptr;
        if (reader.take(4, id) == false) {
            break;
        }
        quint32 size = 0;
        if (reader.u32(size) == false) {
            error = QObject::tr("That WAV file is incomplete.");
            return false;
        }
        if (std::memcmp(id, "fmt ", 4) == 0) {
            const int start = reader.offset;
            quint16 blockAlign = 0;
            if (reader.u16(audioFormat) == false || reader.u16(channels) == false || reader.u32(sampleRate) == false) {
                error = QObject::tr("That WAV file is incomplete.");
                return false;
            }
            quint32 byteRate = 0;
            if (reader.u32(byteRate) == false || reader.u16(blockAlign) == false || reader.u16(bits) == false) {
                error = QObject::tr("That WAV file is incomplete.");
                return false;
            }
            const int consumed = reader.offset - start;
            if (static_cast<quint32>(consumed) < size) {
                const char* skip = nullptr;
                if (reader.take(static_cast<int>(size) - consumed, skip) == false) {
                    error = QObject::tr("That WAV file is incomplete.");
                    return false;
                }
            }
            haveFormat = true;
        } else if (std::memcmp(id, "data", 4) == 0) {
            if (reader.offset + static_cast<int>(size) > bytes.size()) {
                error = QObject::tr("That WAV file is incomplete.");
                return false;
            }
            data = bytes.constData() + reader.offset;
            dataBytes = size;
            break;
        } else {
            const char* skip = nullptr;
            if (reader.take(static_cast<int>(size), skip) == false) {
                error = QObject::tr("That WAV file is incomplete.");
                return false;
            }
        }
        if ((size & 1U) == 1U) {
            const char* pad = nullptr;
            reader.take(1, pad);
        }
    }
    if (haveFormat == false || data == nullptr) {
        error = QObject::tr("That WAV file has no audio.");
        return false;
    }
    if (audioFormat != 1 || bits != 16 || (channels != 1 && channels != 2) || sampleRate < 1) {
        error = QObject::tr("Waterfall plays 16-bit PCM WAV files. This one is a different kind.");
        return false;
    }
    const quint32 frameBytes = static_cast<quint32>(channels) * 2U;
    const quint32 frames = dataBytes / frameBytes;
    clip.sampleRate = static_cast<int>(sampleRate);
    clip.samples.resize(frames);
    for (quint32 frame = 0; frame < frames; ++frame) {
        const unsigned char* raw = reinterpret_cast<const unsigned char*>(data) + static_cast<quint64>(frame) * frameBytes;
        const auto left = static_cast<qint16>(raw[0] | (raw[1] << 8));
        if (channels == 1) {
            clip.samples[frame] = left;
        } else {
            const auto right = static_cast<qint16>(raw[2] | (raw[3] << 8));
            clip.samples[frame] = static_cast<qint16>((static_cast<int>(left) + static_cast<int>(right)) / 2);
        }
    }
    if (clip.samples.empty() == true) {
        error = QObject::tr("That WAV file has no audio.");
        return false;
    }
    return true;
}

// ================================================================================

void appendMp3(const short* left, const short* right, int count, int channels, AudioClip& clip) {
    const size_t base = clip.samples.size();
    clip.samples.resize(base + static_cast<size_t>(count));
    for (int index = 0; index < count; ++index) {
        if (channels == 2) {
            clip.samples[base + static_cast<size_t>(index)] =
                    static_cast<qint16>((static_cast<int>(left[index]) + static_cast<int>(right[index])) / 2);
        } else {
            clip.samples[base + static_cast<size_t>(index)] = left[index];
        }
    }
}

// ================================================================================

bool decodeMp3(const QByteArray& bytes, AudioClip& clip, QString& error) {
    hip_t hip = hip_decode_init();
    if (hip == nullptr) {
        error = QObject::tr("The MP3 decoder could not start.");
        return false;
    }
    mp3data_struct info{};
    std::vector<short> left(1152 * 2);
    std::vector<short> right(1152 * 2);
    auto* data = reinterpret_cast<unsigned char*>(const_cast<char*>(bytes.constData()));
    const auto take = [&](unsigned char* buffer, size_t length) -> bool {
        while (true) {
            const int count = hip_decode1_headers(hip, buffer, length, left.data(), right.data(), &info);
            buffer = nullptr;
            length = 0;
            if (count < 0) {
                return false;
            }
            if (count == 0) {
                return true;
            }
            if (info.samplerate > 0) {
                clip.sampleRate = info.samplerate;
            }
            appendMp3(left.data(), right.data(), count, info.stereo == 2 ? 2 : 1, clip);
        }
    };
    size_t offset = 0;
    while (offset < static_cast<size_t>(bytes.size())) {
        const size_t chunk = std::min(static_cast<size_t>(bytes.size()) - offset, static_cast<size_t>(1024));
        if (take(data + offset, chunk) == false) {
            hip_decode_exit(hip);
            error = QObject::tr("That MP3 file could not be read.");
            return false;
        }
        offset += chunk;
    }
    if (take(nullptr, 0) == false) {
        hip_decode_exit(hip);
        error = QObject::tr("That MP3 file could not be read.");
        return false;
    }
    hip_decode_exit(hip);
    if (clip.samples.empty() == true || clip.sampleRate < 1) {
        error = QObject::tr("That MP3 file has no audio.");
        return false;
    }
    return true;
}
}  // namespace

// ================================================================================

bool AudioClip::decode(const QByteArray& bytes, AudioClip& clip, QString& error) {
    error.clear();
    clip.samples.clear();
    clip.sampleRate = 0;
    if (bytes.isEmpty() == true) {
        error = QObject::tr("That file has no audio.");
        return false;
    }
    if (looksLikeWav(bytes) == true) {
        return decodeWav(bytes, clip, error);
    }
    if (decodeMp3(bytes, clip, error) == true) {
        return true;
    }
    error = QObject::tr("Waterfall plays WAV and MP3 files.");
    return false;
}

// ================================================================================

bool AudioClip::load(const QString& path, AudioClip& clip, QString& error) {
    error.clear();
    QFile file(path);
    if (file.open(QIODevice::ReadOnly) == false) {
        error = file.errorString();
        return false;
    }
    if (file.size() > kMaxFileBytes) {
        error = QObject::tr("That file is too large to play.");
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFile::NoError) {
        error = file.errorString();
        return false;
    }
    return decode(bytes, clip, error);
}
