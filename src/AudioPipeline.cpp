#include "AudioPipeline.h"

#include "AudioRingBuffer.h"
#include "Palette.h"

#include <QAudio>
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

// ================================================================================

int hopFor(int fftSize, int overlapPercent) {
    if (fftSize < 2) {
        return 0;
    }
    const int percent = std::clamp(overlapPercent, 0, 87);
    if (percent >= 87) {
        return std::max(1, fftSize / 8);
    }
    const int hop = fftSize - (fftSize * percent) / 100;
    return std::clamp(hop, 1, fftSize);
}
}  // namespace

// ================================================================================

AudioPipeline::AudioPipeline(QObject* parent) : QObject(parent) {
    qRegisterMetaType<QVector<QRgb>>();
    m_releaseTimer = new QTimer(this);
    m_releaseTimer->setTimerType(Qt::PreciseTimer);
    connect(m_releaseTimer, &QTimer::timeout, this, &AudioPipeline::releaseDueColumns);
}

// ================================================================================

AudioPipeline::~AudioPipeline() {
    stop();
}

// ================================================================================

bool AudioPipeline::start(const QAudioDevice& device, int sampleRate, int fftSize, int overlapPercent,
                          const QString& paletteName, QString& error) {
    stop();
    error.clear();
    if (device.isNull() == true) {
        error = tr("No input device is available.");
        return false;
    }
    prepareOffline(fftSize, paletteName, overlapPercent);
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

void AudioPipeline::prepareOffline(int fftSize, const QString& paletteName, int overlapPercent) {
    if (m_running.load(std::memory_order_acquire) == true) {
        return;
    }
    m_palette = paletteName;
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
        return;
    }
    m_fftSize = fftSize;
    m_hop = hopFor(fftSize, overlapPercent);
    m_fft.configure(fftSize);
    size_t capacity = 131072;
    const size_t needed = static_cast<size_t>(fftSize) * 16;
    while (capacity < needed) {
        capacity *= 2;
    }
    m_buffer = std::make_unique<AudioRingBuffer>(capacity);
    m_block.assign(static_cast<size_t>(fftSize), 0.f);
    m_levels.assign(static_cast<size_t>(m_fft.binCount()), 0.f);
}

// ================================================================================

size_t AudioPipeline::writeSamples(std::span<const float> samples) {
    if (m_buffer == nullptr) {
        return 0;
    }
    return m_buffer->write(samples);
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
    if (m_fftSize < 2 || fillWindow() == false) {
        return false;
    }
    if (m_fft.process(m_block, m_levels) == false) {
        return false;
    }
    const int bins = static_cast<int>(m_levels.size());
    column.resize(bins);
    for (int i = 0; i < bins; ++i) {
        // Image row 0 is the top of the waterfall, so Nyquist goes first.
        const int source = bins - 1 - i;
        column[i] = Palette::color(m_palette, m_levels[static_cast<size_t>(source)]);
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
