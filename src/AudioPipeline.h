#pragma once

#include "FftEngine.h"
#include "PcmHistory.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QObject>
#include <QRgb>
#include <QString>
#include <QVector>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

class QAudioSource;
class QIODevice;
class QTimer;
class AudioRingBuffer;

// Captures one input and turns overlapped FFT blocks into color columns.
// Overlap is the percent of the window reused for the next column (87 means
// 87.5%). Columns are released on the hop interval so a large audio chunk
// scrolls instead of appearing at once. Low frequencies are at the bottom.
// prepareOffline() plus writeSamples() drives the same path without a device.
class AudioPipeline : public QObject {
    Q_OBJECT

public:
    explicit AudioPipeline(QObject* parent = nullptr);
    ~AudioPipeline() override;

    bool start(const QAudioDevice& device, int sampleRate, int fftSize, int overlapPercent, int maxFrequencyHz,
               const QString& paletteName, QString& error);
    void stop();
    bool isRunning() const { return m_running.load(std::memory_order_acquire) == true; }
    int hopSamples() const { return m_hop; }
    int captureSampleRate() const { return m_sampleRate; }

    // How much recent audio Save can write. Applies on the next Start.
    void setRecordingSeconds(int seconds);
    int recordingSeconds() const { return m_recordingSeconds; }
    std::vector<std::int16_t> copyRecording() const;
    int recordingSampleRate() const;
    bool writeRecording(const QString& path, QString& error) const;

    void prepareOffline(int fftSize, const QString& paletteName, int overlapPercent = 0, int sampleRate = 0,
                        int maxFrequencyHz = 0);
    size_t writeSamples(std::span<const float> samples);
    bool processPendingBlock();

signals:
    void columnReady(const QVector<QRgb>& column);

private:
    void onReadyRead();
    void threadMain();
    void consumeBytes(const char* data, int byteCount);
    void releaseDueColumns();
    bool fillWindow();
    bool nextColumn(QVector<QRgb>& column);
    int columnIntervalMs() const;

    std::unique_ptr<AudioRingBuffer> m_buffer;
    FftEngine m_fft;
    QString m_palette;
    int m_fftSize = 0;
    int m_hop = 0;
    int m_filled = 0;
    int m_sampleRate = 0;
    int m_maxFrequencyHz = 0;
    int m_recordingSeconds = 0;
    PcmHistory m_history;
    QAudioFormat m_format;
    std::unique_ptr<QAudioSource> m_source;
    QIODevice* m_io = nullptr;
    QByteArray m_pending;
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    std::vector<float> m_block;
    std::vector<float> m_levels;
    std::mutex m_queueMutex;
    std::deque<QVector<QRgb>> m_readyColumns;
    QTimer* m_releaseTimer = nullptr;
};
