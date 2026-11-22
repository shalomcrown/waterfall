#pragma once

#include "ColumnSource.h"
#include "FftEngine.h"
#include "PcmHistory.h"
#include "SpectrumColumn.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QElapsedTimer>
#include <QObject>
#include <QRgb>
#include <QString>
#include <QVector>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <map>
#include <span>
#include <thread>
#include <vector>

class QAudioSink;
class QAudioSource;
class QIODevice;
class QTimer;
class AudioRingBuffer;

// Captures one input and turns overlapped FFT blocks into color columns.
// Overlap is the percent of the window reused for the next column (87, 94 and 97
// mean 87.5%, 93.75% and 96.875%). Columns are released on the hop interval so a
// large audio chunk scrolls instead of appearing at once. Low frequencies are at the bottom.
// prepareOffline() plus writeSamples() drives the same path without a device.
class AudioPipeline : public QObject, public ColumnSource {
    Q_OBJECT

public:
    explicit AudioPipeline(QObject* parent = nullptr);
    ~AudioPipeline() override;

    bool start(const QAudioDevice& device, int sampleRate, int fftSize, int overlapPercent, int maxFrequencyHz,
               const QString& paletteName, QString& error);
    void stop();
    bool isRunning() const { return m_running.load(std::memory_order_acquire) == true; }

    // A loaded file replaces live capture. Play draws the waterfall from the file
    // and, when an output device is available, plays it there too.
    bool loadClip(std::span<const std::int16_t> samples, int sampleRate, QString& error);
    void clearClip();
    bool clipLoaded() const { return m_clip.empty() == false; }
    bool isPlaying() const { return m_playing; }
    size_t clipPosition() const { return m_clipPos; }
    size_t clipLength() const { return m_clip.size(); }
    int clipSampleRate() const { return m_clipRate; }
    bool playClip(const QAudioDevice& device, int fftSize, int overlapPercent, int maxFrequencyHz,
                  const QString& paletteName, QString& error);
    void pauseClip();
    int hopSamples() const { return m_hop; }
    int fftSamples() const { return m_fftSize; }
    int captureSampleRate() const { return m_sampleRate; }

    // How much recent audio Save can write. The new length applies on the next Start.
    // Stop keeps the samples so they can still be saved.
    void setRecordingSeconds(int seconds);
    int recordingSeconds() const { return m_recordingSeconds; }
    std::vector<std::int16_t> copyRecording() const;
    int recordingSampleRate() const;
    bool writeRecording(const QString& path, QString& error) const;

    void prepareOffline(int fftSize, const QString& paletteName, int overlapPercent = 0, int sampleRate = 0,
                        int maxFrequencyHz = 0, bool keepRecording = false);
    // Display mapping for the next column. It does not restart capture.
    void setFrequencyDisplay(bool logarithmic, int floorHz);
    size_t writeSamples(std::span<const float> samples);
    bool processPendingBlock();

    // ColumnSource. Redraws any column from the kept audio. While a file is
    // playing the source is the whole file, so unplayed parts can be drawn too.
    bool renderColumn(long long windowEnd, int fftSize, int maxRows, QVector<QRgb>& column) const override;
    long long firstPosition() const override;
    long long endPosition() const override;
    long long livePosition() const override;

signals:
    void columnReady(const QVector<QRgb>& column);
    void playbackFinished();

private:
    void onReadyRead();
    void onPlaybackTick();
    void endPlayback();
    void openPlaybackSink(const QAudioDevice& device);
    void threadMain();
    void consumeBytes(const char* data, int byteCount);
    void releaseDueColumns();
    bool fillWindow();
    bool nextColumn(QVector<QRgb>& column);
    SpectrumColumnSettings columnSettings() const;
    int columnIntervalMs() const;

    std::unique_ptr<AudioRingBuffer> m_buffer;
    FftEngine m_fft;
    QString m_palette;
    int m_fftSize = 0;
    int m_hop = 0;
    int m_filled = 0;
    int m_sampleRate = 0;
    int m_maxFrequencyHz = 0;
    std::atomic<bool> m_logarithmic{false};
    std::atomic<int> m_logFloorHz{5};
    std::atomic<int> m_displayGeneration{0};
    int m_recordingSeconds = 0;
    PcmHistory m_history;
    QAudioFormat m_format;
    std::unique_ptr<QAudioSource> m_source;
    QIODevice* m_io = nullptr;
    QByteArray m_pending;
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    std::vector<std::int16_t> m_clip;
    int m_clipRate = 0;
    size_t m_clipPos = 0;
    bool m_playing = false;
    std::unique_ptr<QAudioSink> m_sink;
    QIODevice* m_sinkIo = nullptr;
    QAudioFormat m_playbackFormat;
    QTimer* m_playbackTimer = nullptr;
    QElapsedTimer m_playbackClock;
    qint64 m_playbackOriginSamples = 0;
    std::vector<float> m_block;
    std::vector<float> m_levels;
    // The redraw path has its own FFTs, one per window length it has been asked for,
    // and its own scratch space, so it never shares state with the capture thread. It
    // runs on the UI thread only.
    mutable std::map<int, FftEngine> m_renderEngines;
    mutable std::vector<float> m_renderBlock;
    mutable std::vector<float> m_renderLevels;
    bool m_renderFromClip = false;
    bool m_renderValid = false;
    std::mutex m_queueMutex;
    std::deque<QVector<QRgb>> m_readyColumns;
    QTimer* m_releaseTimer = nullptr;
};
