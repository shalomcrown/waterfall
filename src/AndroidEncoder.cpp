#include "AndroidEncoder.h"

#include <QtGlobal>

#include <QByteArray>
#include <QDebug>
#include <QJniEnvironment>
#include <QJniObject>
#include <QObject>

#include <algorithm>

namespace {
constexpr int kBufferFlagEndOfStream = 4;
constexpr int kBufferFlagCodecConfig = 2;
constexpr int kInfoOutputFormatChanged = -2;
constexpr int kConfigureFlagEncode = 1;
constexpr int kAacObjectLc = 2;
constexpr int kEncodingPcm16Bit = 2;
constexpr int kMuxerMpeg4 = 0;
constexpr int kMuxerOgg = 4;
constexpr int kAacBitrate = 128000;
constexpr int kOpusBitrate = 64000;
constexpr int kPcmEncodingSdk = 24;
constexpr int kIdleLimit = 500;
constexpr int kEmptyOutputLimit = 1000;
constexpr jlong kCodecTimeoutUs = 10000;

struct CodecPlan {
    QString mime;
    int muxerFormat = 0;
    int bitrate = 0;
    bool aacProfile = false;
};

// ================================================================================

bool codecPlan(const QString& formatId, CodecPlan& plan) {
    if (formatId == QStringLiteral("m4a")) {
        plan.mime = QStringLiteral("audio/mp4a-latm");
        plan.muxerFormat = kMuxerMpeg4;
        plan.bitrate = kAacBitrate;
        plan.aacProfile = true;
        return true;
    }
    if (formatId == QStringLiteral("ogg")) {
        plan.mime = QStringLiteral("audio/opus");
        plan.muxerFormat = kMuxerOgg;
        plan.bitrate = kOpusBitrate;
        plan.aacProfile = false;
        return true;
    }
    return false;
}

// ================================================================================

void setInteger(QJniObject& format, const QString& key, int value) {
    format.callMethod<void>("setInteger", "(Ljava/lang/String;I)V", QJniObject::fromString(key).object<jstring>(),
                            static_cast<jint>(value));
}

// ================================================================================

bool queuePcm(QJniEnvironment& env, const QJniObject& codec, jint index, std::span<const std::int16_t> samples,
              size_t offset, int sampleRate, qint64 queuedSamples, bool endOfStream, size_t& consumed, QString& error) {
    QJniObject buffer = codec.callObjectMethod("getInputBuffer", "(I)Ljava/nio/ByteBuffer;", index);
    if (androidTakeJavaException(error, QObject::tr("This device could not encode that format. Save as WAV instead.")) ||
        buffer.isValid() == false) {
        if (error.isEmpty() == true) {
            error = QObject::tr("This device could not encode that format. Save as WAV instead.");
        }
        return false;
    }
    buffer.callObjectMethod("clear", "()Ljava/nio/Buffer;");
    if (androidTakeJavaException(error, QObject::tr("This device could not encode that format. Save as WAV instead."))) {
        return false;
    }
    jint size = 0;
    if (endOfStream == false) {
        const jint capacity = buffer.callMethod<jint>("capacity", "()I");
        if (capacity < 2) {
            error = QObject::tr("This device could not encode that format. Save as WAV instead.");
            return false;
        }
        const size_t room = static_cast<size_t>(capacity / 2);
        const size_t remaining = samples.size() - offset;
        const size_t count = std::min(room, remaining);
        QByteArray pcm(static_cast<qsizetype>(count * 2), '\0');
        char* out = pcm.data();
        for (size_t i = 0; i < count; ++i) {
            const auto value = static_cast<quint16>(samples[offset + i]);
            *out++ = static_cast<char>(value & 0xff);
            *out++ = static_cast<char>((value >> 8) & 0xff);
        }
        jbyteArray bytes = env->NewByteArray(static_cast<jsize>(pcm.size()));
        env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(pcm.size()), reinterpret_cast<const jbyte*>(pcm.constData()));
        buffer.callObjectMethod("put", "([B)Ljava/nio/ByteBuffer;", bytes);
        env->DeleteLocalRef(bytes);
        if (androidTakeJavaException(error, QObject::tr("This device could not encode that format. Save as WAV instead."))) {
            return false;
        }
        size = static_cast<jint>(count * 2);
        consumed = count;
    }
    const jlong when = queuedSamples * 1000000LL / sampleRate;
    const jint flags = endOfStream ? kBufferFlagEndOfStream : 0;
    codec.callMethod<void>("queueInputBuffer", "(IIIJI)V", index, jint{0}, size, when, flags);
    return androidTakeJavaException(error, QObject::tr("This device could not encode that format. Save as WAV instead.")) == false;
}

// ================================================================================

struct CodecSession {
    QJniObject codec;
    QJniObject muxer;
    bool codecStarted = false;
    bool muxerStarted = false;
    bool muxerStopped = false;

    ~CodecSession() {
        QJniEnvironment env;
        env->ExceptionClear();
        if (muxer.isValid() == true) {
            if (muxerStarted == true && muxerStopped == false) {
                muxer.callMethod<void>("stop", "()V");
                env->ExceptionClear();
            }
            muxer.callMethod<void>("release", "()V");
            env->ExceptionClear();
        }
        if (codec.isValid() == true) {
            if (codecStarted == true) {
                codec.callMethod<void>("stop", "()V");
                env->ExceptionClear();
            }
            codec.callMethod<void>("release", "()V");
            env->ExceptionClear();
        }
    }
};
}  // namespace

// ================================================================================

bool androidTakeJavaException(QString& error, const QString& fallback) {
    QJniEnvironment env;
    if (env->ExceptionCheck() == JNI_FALSE) {
        return false;
    }
    QJniObject thrown(env->ExceptionOccurred());
    env->ExceptionClear();
    QString detail;
    if (thrown.isValid() == true) {
        detail = thrown.callObjectMethod("getMessage", "()Ljava/lang/String;").toString();
        if (detail.isEmpty() == true) {
            detail = thrown.callObjectMethod("toString", "()Ljava/lang/String;").toString();
        }
    }
    error = detail.isEmpty() ? fallback : QStringLiteral("%1 %2").arg(fallback, detail);
    qWarning("Recording save: %s", qPrintable(error));
    return true;
}

// ================================================================================

int androidSdkInt() {
    return QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");
}

// ================================================================================

bool androidEncoderAvailable(const QString& mimeType) {
    QJniEnvironment env;
    QString ignored;
    QJniObject list("android/media/MediaCodecList", "(I)V", jint{0});
    if (androidTakeJavaException(ignored, QString()) == true || list.isValid() == false) {
        return false;
    }
    QJniObject infos = list.callObjectMethod("getCodecInfos", "()[Landroid/media/MediaCodecInfo;");
    if (androidTakeJavaException(ignored, QString()) == true || infos.isValid() == false) {
        return false;
    }
    jobjectArray infoArray = infos.object<jobjectArray>();
    if (infoArray == nullptr) {
        return false;
    }
    const jsize infoCount = env->GetArrayLength(infoArray);
    for (jsize i = 0; i < infoCount; ++i) {
        jobject rawInfo = env->GetObjectArrayElement(infoArray, i);
        QJniObject info(rawInfo);
        env->DeleteLocalRef(rawInfo);
        if (info.isValid() == false || info.callMethod<jboolean>("isEncoder", "()Z") == JNI_FALSE) {
            continue;
        }
        QJniObject types = info.callObjectMethod("getSupportedTypes", "()[Ljava/lang/String;");
        if (types.isValid() == false) {
            continue;
        }
        jobjectArray typeArray = types.object<jobjectArray>();
        if (typeArray == nullptr) {
            continue;
        }
        const jsize typeCount = env->GetArrayLength(typeArray);
        for (jsize typeIndex = 0; typeIndex < typeCount; ++typeIndex) {
            jobject rawType = env->GetObjectArrayElement(typeArray, typeIndex);
            QJniObject type(rawType);
            env->DeleteLocalRef(rawType);
            if (type.toString() == mimeType) {
                return true;
            }
        }
    }
    return false;
}

// ================================================================================

bool androidEncodeFile(const QString& path, const QString& formatId, int sampleRate,
                       std::span<const std::int16_t> samples, QString& error) {
    error.clear();
    const QString failed = QObject::tr("This device could not encode that format. Save as WAV instead.");
    CodecPlan plan;
    if (codecPlan(formatId, plan) == false || sampleRate < 1 || samples.empty() == true) {
        error = failed;
        return false;
    }

    CodecSession session;
    session.codec = QJniObject::callStaticObjectMethod(
            "android/media/MediaCodec", "createEncoderByType", "(Ljava/lang/String;)Landroid/media/MediaCodec;",
            QJniObject::fromString(plan.mime).object<jstring>());
    if (androidTakeJavaException(error, failed) == true || session.codec.isValid() == false) {
        if (error.isEmpty() == true) {
            error = failed;
        }
        return false;
    }

    QJniObject format = QJniObject::callStaticObjectMethod(
            "android/media/MediaFormat", "createAudioFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;",
            QJniObject::fromString(plan.mime).object<jstring>(), static_cast<jint>(sampleRate), jint{1});
    if (androidTakeJavaException(error, failed) == true || format.isValid() == false) {
        if (error.isEmpty() == true) {
            error = failed;
        }
        return false;
    }
    setInteger(format, QStringLiteral("bitrate"), plan.bitrate);
    if (androidTakeJavaException(error, failed) == true) {
        return false;
    }
    setInteger(format, QStringLiteral("max-input-size"), 16384);
    if (androidTakeJavaException(error, failed) == true) {
        return false;
    }
    if (plan.aacProfile == true) {
        setInteger(format, QStringLiteral("aac-profile"), kAacObjectLc);
        if (androidTakeJavaException(error, failed) == true) {
            return false;
        }
    }
    if (androidSdkInt() >= kPcmEncodingSdk) {
        setInteger(format, QStringLiteral("pcm-encoding"), kEncodingPcm16Bit);
        if (androidTakeJavaException(error, failed) == true) {
            return false;
        }
    }

    session.codec.callMethod<void>("configure", "(Landroid/media/MediaFormat;Landroid/view/Surface;Landroid/media/MediaCrypto;I)V",
                                  format.object(), jobject{nullptr}, jobject{nullptr}, static_cast<jint>(kConfigureFlagEncode));
    if (androidTakeJavaException(error, failed) == true) {
        return false;
    }
    session.codec.callMethod<void>("start", "()V");
    if (androidTakeJavaException(error, failed) == true) {
        return false;
    }
    session.codecStarted = true;

    session.muxer = QJniObject("android/media/MediaMuxer", "(Ljava/lang/String;I)V",
                               QJniObject::fromString(path).object<jstring>(), static_cast<jint>(plan.muxerFormat));
    if (androidTakeJavaException(error, failed) == true || session.muxer.isValid() == false) {
        if (error.isEmpty() == true) {
            error = failed;
        }
        return false;
    }

    QJniEnvironment env;
    QJniObject bufferInfo("android/media/MediaCodec$BufferInfo");
    if (androidTakeJavaException(error, failed) == true || bufferInfo.isValid() == false) {
        if (error.isEmpty() == true) {
            error = failed;
        }
        return false;
    }

    bool inputDone = false;
    size_t sampleOffset = 0;
    qint64 queuedSamples = 0;
    int track = -1;
    int idle = 0;
    int emptyOutputs = 0;
    while (true) {
        if (inputDone == false) {
            const jint inIndex = session.codec.callMethod<jint>("dequeueInputBuffer", "(J)I", kCodecTimeoutUs);
            if (androidTakeJavaException(error, failed) == true) {
                return false;
            }
            if (inIndex >= 0) {
                idle = 0;
                const bool endOfStream = sampleOffset >= samples.size();
                size_t consumed = 0;
                if (queuePcm(env, session.codec, inIndex, samples, sampleOffset, sampleRate, queuedSamples, endOfStream,
                             consumed, error) == false) {
                    return false;
                }
                queuedSamples += static_cast<qint64>(consumed);
                sampleOffset += consumed;
                if (endOfStream == true) {
                    inputDone = true;
                }
            }
        }

        const jint outIndex = session.codec.callMethod<jint>(
                "dequeueOutputBuffer", "(Landroid/media/MediaCodec$BufferInfo;J)I", bufferInfo.object(), kCodecTimeoutUs);
        if (androidTakeJavaException(error, failed) == true) {
            return false;
        }
        if (outIndex == kInfoOutputFormatChanged) {
            idle = 0;
            QJniObject encoded = session.codec.callObjectMethod("getOutputFormat", "()Landroid/media/MediaFormat;");
            if (androidTakeJavaException(error, failed) == true || encoded.isValid() == false) {
                if (error.isEmpty() == true) {
                    error = failed;
                }
                return false;
            }
            track = session.muxer.callMethod<jint>("addTrack", "(Landroid/media/MediaFormat;)I", encoded.object());
            if (androidTakeJavaException(error, failed) == true) {
                return false;
            }
            session.muxer.callMethod<void>("start", "()V");
            if (androidTakeJavaException(error, failed) == true) {
                return false;
            }
            session.muxerStarted = true;
            continue;
        }
        if (outIndex < 0) {
            idle += 1;
            if (idle > kIdleLimit) {
                error = failed;
                return false;
            }
            continue;
        }

        idle = 0;
        const jint flags = bufferInfo.getField<jint>("flags");
        const jint size = bufferInfo.getField<jint>("size");
        const jint offset = bufferInfo.getField<jint>("offset");
        if (size == 0 && (flags & kBufferFlagEndOfStream) == 0) {
            emptyOutputs += 1;
            if (emptyOutputs > kEmptyOutputLimit) {
                error = failed;
                return false;
            }
        } else {
            emptyOutputs = 0;
        }
        if (session.muxerStarted == true && size > 0 && (flags & kBufferFlagCodecConfig) == 0) {
            QJniObject encoded = session.codec.callObjectMethod("getOutputBuffer", "(I)Ljava/nio/ByteBuffer;", outIndex);
            if (encoded.isValid() == true) {
                encoded.callObjectMethod("position", "(I)Ljava/nio/Buffer;", offset);
                encoded.callObjectMethod("limit", "(I)Ljava/nio/Buffer;", offset + size);
                session.muxer.callMethod<void>("writeSampleData",
                                              "(ILjava/nio/ByteBuffer;Landroid/media/MediaCodec$BufferInfo;)V", track,
                                              encoded.object(), bufferInfo.object());
            }
            if (androidTakeJavaException(error, failed) == true) {
                return false;
            }
        }
        session.codec.callMethod<void>("releaseOutputBuffer", "(IZ)V", outIndex, jboolean{JNI_FALSE});
        if (androidTakeJavaException(error, failed) == true) {
            return false;
        }
        if ((flags & kBufferFlagEndOfStream) != 0) {
            break;
        }
    }

    if (session.muxerStarted == false) {
        error = failed;
        return false;
    }
    session.muxer.callMethod<void>("stop", "()V");
    if (androidTakeJavaException(error, failed) == true) {
        return false;
    }
    session.muxerStopped = true;
    return true;
}
