#include "AudioPipeline.h"

#include "AudioRingBuffer.h"
#include "FftTiming.h"
#include "Palette.h"
#include "WavFile.h"

#include <QAudio>
#include <QByteArray>
#include <QAudioSink>
#include <QAudioSource>
#include <QIODevice>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
QAudioFormat captureFormat(const QAudioDevice& device, int sampleRate) {
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);
    if (device.isFormatSupported(format) == true) {
        return format;
    }
    format.setSampleFormat(QAudioFormat::Int16);
    if (device.isFormatSupported(format) == true) {
        return format;
    }
    return device.preferredFormat();
}
}  // namespace

// ================================================================================

AudioPipeline::AudioPipeline(QObject* parent) : QObject(parent) {
    qRegisterMetaType<QVector<QRgb>>();
    m_releaseTimer = new QTimer(this);
    m_releaseTimer->setTimerType(Qt::PreciseTimer);
    connect(m_releaseTimer, &QTimer::timeout, this, &AudioPipeline::releaseDueColumns);
    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setTimerType(Qt::PreciseTimer);
    connect(m_playbackTimer, &QTimer::timeout, this, &AudioPipeline::onPlaybackTick);
}

// ================================================================================

AudioPipeline::~AudioPipeline() {
    stop();
}

// ================================================================================

bool AudioPipeline::start(const QAudioDevice& device, int sampleRate, int fftSize, int overlapPercent,
                          int maxFrequencyHz, const QString& paletteName, QString& error) {
    stop();
    error.clear();
    if (device.isNull() == true) {
        error = tr("No input device is available.");
        return false;
    }
    prepareOffline(fftSize, paletteName, overlapPercent, sampleRate, maxFrequencyHz, true);
    if (m_fftSize < 2 || m_hop < 1) {
        error = tr("The FFT size is too small.");
        return false;
    }

    m_format = captureFormat(device, sampleRate);
    if (device.isFormatSupported(m_format) == false || m_format.bytesPerFrame() < 1) {
        error = tr("The input device rejected the audio format.");
        return false;
    }
    m_sampleRate = m_format.sampleRate() > 0 ? m_format.sampleRate() : sampleRate;

    m_source = std::make_unique<QAudioSource>(device, m_format);
    const int periodSamples = std::max(m_hop * 2, m_sampleRate / 50);
    const int bufferBytes = std::max(periodSamples, m_fftSize) * m_format.bytesPerFrame();
    m_source->setBufferSize(bufferBytes);
    m_io = m_source->start();
    if (m_io == nullptr || m_source->error() != QAudio::NoError) {
        m_source.reset();
        m_io = nullptr;
        error = tr("The input device did not start.");
        return false;
    }
    m_history.reset(m_sampleRate, m_recordingSeconds);
    connect(m_io, &QIODevice::readyRead, this, &AudioPipeline::onReadyRead);
    const int hopMs = columnIntervalMs();
    m_releaseTimer->setInterval(hopMs < 16 ? 16 : hopMs);
    m_releaseTimer->start();
    m_running.store(true, std::memory_order_release);
    m_thread = std::thread([this] { threadMain(); });
    return true;
}

// ================================================================================

void AudioPipeline::stop() {
    endPlayback();
    if (m_releaseTimer != nullptr) {
        m_releaseTimer->stop();
    }
    if (m_io != nullptr) {
        disconnect(m_io, nullptr, this, nullptr);
        m_io = nullptr;
    }
    if (m_source != nullptr) {
        m_source->stop();
        m_source.reset();
    }
    m_running.store(false, std::memory_order_release);
    if (m_thread.joinable() == true) {
        m_thread.join();
    }
    m_pending.clear();
    m_filled = 0;
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_readyColumns.clear();
}

// ================================================================================

void AudioPipeline::prepareOffline(int fftSize, const QString& paletteName, int overlapPercent, int sampleRate,
                                  int maxFrequencyHz, bool keepRecording) {
    if (m_running.load(std::memory_order_acquire) == true) {
        return;
    }
    m_palette = paletteName;
    m_sampleRate = sampleRate;
    m_maxFrequencyHz = maxFrequencyHz;
    m_pending.clear();
    m_filled = 0;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_readyColumns.clear();
    }
    if (fftSize < 2) {
        m_fftSize = 0;
        m_hop = 0;
        m_fft.configure(0);
        m_buffer.reset();
        m_block.clear();
        m_levels.clear();
        if (keepRecording == false) {
            m_history.clear();
        }
        return;
    }
    m_fftSize = fftSize;
    m_hop = waterfallHop(fftSize, overlapPercent);
    m_fft.configure(fftSize);
    size_t capacity = 131072;
    const size_t needed = static_cast<size_t>(fftSize) * 16;
    while (capacity < needed) {
        capacity *= 2;
    }
    m_buffer = std::make_unique<AudioRingBuffer>(capacity);
    m_block.assign(static_cast<size_t>(fftSize), 0.f);
    m_levels.assign(static_cast<size_t>(m_fft.binCount()), 0.f);
    if (keepRecording == false) {
        m_history.reset(m_sampleRate, m_recordingSeconds);
    }
}

// ================================================================================

size_t AudioPipeline::writeSamples(std::span<const float> samples) {
    m_history.append(samples);
    if (m_buffer == nullptr) {
        return 0;
    }
    return m_buffer->write(samples);
}

// ================================================================================

void AudioPipeline::setFrequencyDisplay(bool logarithmic, int floorHz) {
    m_logarithmic.store(logarithmic, std::memory_order_release);
    m_logFloorHz.store(floorHz > 0 ? floorHz : 5, std::memory_order_release);
    m_displayGeneration.fetch_add(1, std::memory_order_acq_rel);
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_readyColumns.clear();
}

// ================================================================================

void AudioPipeline::setRecordingSeconds(int seconds) {
    m_recordingSeconds = seconds > 0 ? seconds : 0;
}

// ================================================================================

std::vector<std::int16_t> AudioPipeline::copyRecording() const {
    return m_history.samples();
}

// ================================================================================

int AudioPipeline::recordingSampleRate() const {
    return m_history.sampleRate();
}

// ================================================================================

bool AudioPipeline::writeRecording(const QString& path, QString& error) const {
    const std::vector<std::int16_t> samples = m_history.samples();
    const int rate = m_history.sampleRate();
    if (samples.empty() == true || rate < 1) {
        error = tr("There is no audio to save.");
        return false;
    }
    return WavFile::write(path, rate, samples, error);
}

// ================================================================================

bool AudioPipeline::processPendingBlock() {
    if (m_running.load(std::memory_order_acquire) == true) {
        return false;
    }
    QVector<QRgb> column;
    if (nextColumn(column) == false) {
        return false;
    }
    emit columnReady(column);
    return true;
}

// ================================================================================

bool AudioPipeline::fillWindow() {
    if (m_buffer == nullptr || m_hop < 1) {
        return false;
    }
    if (m_hop >= m_fftSize) {
        if (m_buffer->availableForRead() < static_cast<size_t>(m_fftSize)) {
            return false;
        }
        const size_t got = m_buffer->read(m_block);
        return got == static_cast<size_t>(m_fftSize);
    }
    if (m_filled < m_fftSize) {
        const int need = m_fftSize - m_filled;
        if (m_buffer->availableForRead() < static_cast<size_t>(need)) {
            return false;
        }
        const size_t got = m_buffer->read(std::span<float>(m_block.data() + m_filled, static_cast<size_t>(need)));
        if (got != static_cast<size_t>(need)) {
            return false;
        }
        m_filled = m_fftSize;
        return true;
    }
    if (m_buffer->availableForRead() < static_cast<size_t>(m_hop)) {
        return false;
    }
    std::move(m_block.begin() + m_hop, m_block.end(), m_block.begin());
    const size_t got =
            m_buffer->read(std::span<float>(m_block.data() + (m_fftSize - m_hop), static_cast<size_t>(m_hop)));
    return got == static_cast<size_t>(m_hop);
}

// ================================================================================

bool AudioPipeline::nextColumn(QVector<QRgb>& column) {
    const int generation = m_displayGeneration.load(std::memory_order_acquire);
    if (m_fftSize < 2 || fillWindow() == false) {
        return false;
    }
    if (m_fft.process(m_block, m_levels) == false) {
        return false;
    }
    const int bins = static_cast<int>(m_levels.size());
    int lastBin = bins - 1;
    if (m_sampleRate > 1 && m_maxFrequencyHz > 0 &&
        static_cast<double>(m_maxFrequencyHz) < static_cast<double>(m_sampleRate) / 2.0) {
        const int limited = static_cast<int>(
                std::lround(static_cast<double>(m_maxFrequencyHz) * m_fftSize / static_cast<double>(m_sampleRate)));
        lastBin = std::clamp(limited, 1, bins - 1);
    }
    const int shown = lastBin + 1;
    column.resize(shown);
    const double topHz =
            m_sampleRate > 1 ? static_cast<double>(lastBin) * static_cast<double>(m_sampleRate) / static_cast<double>(m_fftSize)
                             : 0.0;
    const int floorHz = m_logFloorHz.load(std::memory_order_acquire);
    const bool logarithmic = m_logarithmic.load(std::memory_order_acquire) == true && topHz > static_cast<double>(floorHz);
    if (logarithmic == true) {
        const auto levelAt = [&](double hz) {
            const double bin = hz * static_cast<double>(m_fftSize) / static_cast<double>(m_sampleRate);
            if (bin <= 0.0) {
                return m_levels.front();
            }
            if (bin >= static_cast<double>(bins - 1)) {
                return m_levels.back();
            }
            const int lower = static_cast<int>(std::floor(bin));
            const float fraction = static_cast<float>(bin - static_cast<double>(lower));
            return m_levels[static_cast<size_t>(lower)] * (1.f - fraction) +
                   m_levels[static_cast<size_t>(lower + 1)] * fraction;
        };
        for (int i = 0; i < shown; ++i) {
            const double fromBottom =
                    shown == 1 ? 1.0 : 1.0 - static_cast<double>(i) / static_cast<double>(shown - 1);
            const double hz = hzFromBottom(fromBottom, topHz, true, static_cast<double>(floorHz));
            column[i] = Palette::color(m_palette, levelAt(hz));
        }
    } else {
        for (int i = 0; i < shown; ++i) {
            // Row 0 is the top of the waterfall: the highest displayed frequency.
            const int source = lastBin - i;
            column[i] = Palette::color(m_palette, m_levels[static_cast<size_t>(source)]);
        }
    }
    if (generation != m_displayGeneration.load(std::memory_order_acquire)) {
        column.clear();
        return false;
    }
    return true;
}

// ================================================================================

int AudioPipeline::columnIntervalMs() const {
    if (m_sampleRate < 1 || m_hop < 1) {
        return 20;
    }
    const double milliseconds = 1000.0 * static_cast<double>(m_hop) / static_cast<double>(m_sampleRate);
    return std::max(1, static_cast<int>(std::lround(milliseconds)));
}

// ================================================================================

void AudioPipeline::releaseDueColumns() {
    const int hopMs = std::max(1, columnIntervalMs());
    const int tickMs = std::max(1, m_releaseTimer->interval());
    int budget = std::max(1, (tickMs + hopMs / 2) / hopMs);
    while (budget > 0) {
        QVector<QRgb> column;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_readyColumns.empty() == true) {
                return;
            }
            column = std::move(m_readyColumns.front());
            m_readyColumns.pop_front();
        }
        emit columnReady(column);
        --budget;
    }
}

// ================================================================================

bool AudioPipeline::loadClip(std::span<const std::int16_t> samples, int sampleRate, QString& error) {
    error.clear();
    if (samples.empty() == true || sampleRate < 1) {
        error = tr("That file has no audio.");
        return false;
    }
    stop();
    m_clip.assign(samples.begin(), samples.end());
    m_clipRate = sampleRate;
    m_clipPos = 0;
    return true;
}

// ================================================================================

void AudioPipeline::clearClip() {
    endPlayback();
    m_clip.clear();
    m_clipRate = 0;
    m_clipPos = 0;
}

// ================================================================================

void AudioPipeline::pauseClip() {
    endPlayback();
}

// ================================================================================

void AudioPipeline::endPlayback() {
    m_playing = false;
    if (m_playbackTimer != nullptr) {
        m_playbackTimer->stop();
    }
    m_sinkIo = nullptr;
    if (m_sink != nullptr) {
        m_sink->stop();
        m_sink.reset();
    }
}

// ================================================================================

void AudioPipeline::openPlaybackSink(const QAudioDevice& device) {
    if (device.isNull() == true || m_clipRate < 1) {
        return;
    }
    QAudioFormat format;
    format.setSampleRate(m_clipRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);
    if (device.isFormatSupported(format) == false) {
        format.setSampleFormat(QAudioFormat::Int16);
        if (device.isFormatSupported(format) == false) {
            return;
        }
    }
    m_playbackFormat = format;
    m_sink = std::make_unique<QAudioSink>(device, format);
    const int frameBytes = std::max(1, format.bytesPerFrame());
    m_sink->setBufferSize(std::max(m_clipRate / 5, 1024) * frameBytes);
    m_sinkIo = m_sink->start();
    if (m_sinkIo == nullptr || m_sink->error() != QAudio::NoError) {
        m_sinkIo = nullptr;
        m_sink.reset();
    }
}

// ================================================================================

bool AudioPipeline::playClip(const QAudioDevice& device, int fftSize, int overlapPercent, int maxFrequencyHz,
                             const QString& paletteName, QString& error) {
    error.clear();
    if (m_clip.empty() == true || m_clipRate < 1) {
        error = tr("There is no file to play.");
        return false;
    }
    if (m_playing == true) {
        return true;
    }
    if (m_running.load(std::memory_order_acquire) == true) {
        stop();
    }
    const bool restart = m_clipPos == 0 || m_clipPos >= m_clip.size();
    if (restart == true) {
        m_clipPos = 0;
        prepareOffline(fftSize, paletteName, overlapPercent, m_clipRate, maxFrequencyHz, false);
        if (m_fftSize < 2 || m_hop < 1) {
            error = tr("The FFT size is too small.");
            return false;
        }
    }
    openPlaybackSink(device);
    m_playing = true;
    m_playbackClock.restart();
    m_playbackOriginSamples = static_cast<qint64>(m_clipPos);
    m_playbackTimer->start(20);
    return true;
}

// ================================================================================

void AudioPipeline::onPlaybackTick() {
    if (m_playing == false || m_clipPos >= m_clip.size() || m_clipRate < 1) {
        if (m_playing == true) {
            endPlayback();
            emit playbackFinished();
        }
        return;
    }
    const size_t remaining = m_clip.size() - m_clipPos;
    int count = 0;
    if (m_sinkIo != nullptr && m_playbackFormat.bytesPerFrame() > 0) {
        const int frameBytes = m_playbackFormat.bytesPerFrame();
        const int freeFrames = static_cast<int>(m_sink->bytesFree() / frameBytes);
        count = std::min(freeFrames, std::max(1, m_clipRate / 25));
    } else {
        const qint64 due = m_playbackClock.elapsed() * static_cast<qint64>(m_clipRate) / 1000;
        const qint64 target = m_playbackOriginSamples + due;
        count = static_cast<int>(std::max(qint64{0}, target - static_cast<qint64>(m_clipPos)));
        count = std::min(count, std::max(1, m_clipRate / 10));
    }
    count = std::min(count, static_cast<int>(remaining));
    if (count <= 0) {
        return;
    }
    std::vector<float> floats(static_cast<size_t>(count));
    QByteArray pcm;
    const int frameBytes = m_sinkIo == nullptr ? 0 : m_playbackFormat.bytesPerFrame();
    if (frameBytes > 0) {
        pcm.resize(count * frameBytes);
    }
    for (int index = 0; index < count; ++index) {
        const std::int16_t sample = m_clip[m_clipPos + static_cast<size_t>(index)];
        const float value = static_cast<float>(sample) / 32768.f;
        floats[static_cast<size_t>(index)] = value;
        if (frameBytes == static_cast<int>(sizeof(float))) {
            std::memcpy(pcm.data() + static_cast<qint64>(index) * frameBytes, &value, sizeof(float));
        } else if (frameBytes == static_cast<int>(sizeof(qint16))) {
            std::memcpy(pcm.data() + static_cast<qint64>(index) * frameBytes, &sample, sizeof(sample));
        }
    }
    int played = count;
    if (m_sinkIo != nullptr && pcm.isEmpty() == false) {
        const qint64 written = m_sinkIo->write(pcm);
        if (written < frameBytes) {
            return;
        }
        played = static_cast<int>(written / frameBytes);
    }
    writeSamples(std::span<const float>(floats.data(), static_cast<size_t>(played)));
    while (processPendingBlock() == true) {
    }
    m_clipPos += static_cast<size_t>(played);
    if (m_clipPos >= m_clip.size()) {
        endPlayback();
        emit playbackFinished();
    }
}

// ================================================================================

void AudioPipeline::onReadyRead() {
    if (m_io == nullptr) {
        return;
    }
    m_pending.append(m_io->readAll());
    const int frameBytes = m_format.bytesPerFrame();
    if (frameBytes < 1) {
        return;
    }
    const int frames = m_pending.size() / frameBytes;
    if (frames < 1) {
        return;
    }
    const int used = frames * frameBytes;
    consumeBytes(m_pending.constData(), used);
    m_pending.remove(0, used);
}

// ================================================================================

void AudioPipeline::threadMain() {
    while (m_running.load(std::memory_order_acquire) == true) {
        QVector<QRgb> column;
        if (nextColumn(column) == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_readyColumns.push_back(std::move(column));
        const int maxQueued = std::max(8, m_sampleRate > 0 && m_hop > 0 ? m_sampleRate / m_hop : 32);
        while (static_cast<int>(m_readyColumns.size()) > maxQueued) {
            m_readyColumns.pop_front();
        }
    }
}

// ================================================================================

void AudioPipeline::consumeBytes(const char* data, int byteCount) {
    const int channels = m_format.channelCount();
    const int frameBytes = m_format.bytesPerFrame();
    if (channels < 1 || frameBytes < 1 || data == nullptr) {
        return;
    }
    const int frames = byteCount / frameBytes;
    std::vector<float> samples(static_cast<size_t>(frames));
    for (int frame = 0; frame < frames; ++frame) {
        const char* frameData = data + static_cast<std::ptrdiff_t>(frame) * frameBytes;
        float value = 0.f;
        switch (m_format.sampleFormat()) {
        case QAudioFormat::Float: {
            float sample = 0.f;
            std::memcpy(&sample, frameData, sizeof(float));
            value = sample;
            break;
        }
        case QAudioFormat::Int16: {
            qint16 sample = 0;
            std::memcpy(&sample, frameData, sizeof(sample));
            value = static_cast<float>(sample) / 32768.f;
            break;
        }
        case QAudioFormat::Int32: {
            qint32 sample = 0;
            std::memcpy(&sample, frameData, sizeof(sample));
            value = static_cast<float>(sample) / 2147483648.f;
            break;
        }
        case QAudioFormat::UInt8: {
            const auto sample = static_cast<unsigned char>(frameData[0]);
            value = (static_cast<float>(sample) - 128.f) / 128.f;
            break;
        }
        default:
            value = 0.f;
            break;
        }
        samples[static_cast<size_t>(frame)] = value;
    }
    writeSamples(samples);
}
