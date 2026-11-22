#include "AppSettings.h"
#include "AudioClip.h"
#include "AudioPipeline.h"
#include "AudioRingBuffer.h"
#include "FftEngine.h"
#include "FftTiming.h"
#include "Palette.h"
#include "PcmHistory.h"
#include "Mp3File.h"
#include "RecordingSave.h"
#include "SpectrumColumn.h"
#include "WavFile.h"

#include <QCoreApplication>
#include <QDate>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QDateTime>
#include <QFile>
#include <QRgb>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <span>
#include <thread>
#include <vector>

namespace {
QCoreApplication& audioApp() {
    if (QCoreApplication::instance() != nullptr) {
        return *QCoreApplication::instance();
    }
    static int argc = 1;
    static char arg0[] = "waterfall_tests";
    static char* argv[] = {arg0, nullptr};
    static auto* app = new QCoreApplication(argc, argv);
    return *app;
}

// ================================================================================

std::vector<float> sine(int size, int bin) {
    constexpr double kPi = 3.14159265358979323846;
    std::vector<float> input(static_cast<size_t>(size));
    for (int i = 0; i < size; ++i) {
        input[static_cast<size_t>(i)] = static_cast<float>(std::sin(2.0 * kPi * bin * i / size));
    }
    return input;
}
}  // namespace

// ================================================================================

TEST(AudioRingBuffer, WritesAndReadsAcrossTheWrap) {
    AudioRingBuffer buffer(8);
    EXPECT_EQ(buffer.capacity(), 8u);
    const float first[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    EXPECT_EQ(buffer.write(first), 6u);
    EXPECT_EQ(buffer.availableForRead(), 6u);
    EXPECT_EQ(buffer.availableForWrite(), 2u);

    float readback[6] = {};
    EXPECT_EQ(buffer.read(std::span<float>(readback, 4)), 4u);
    EXPECT_FLOAT_EQ(readback[0], 1.f);
    EXPECT_FLOAT_EQ(readback[3], 4.f);

    const float second[] = {7.f, 8.f, 9.f, 10.f};
    EXPECT_EQ(buffer.write(second), 4u);
    EXPECT_EQ(buffer.read(std::span<float>(readback, 6)), 6u);
    EXPECT_FLOAT_EQ(readback[0], 5.f);
    EXPECT_FLOAT_EQ(readback[5], 10.f);
    EXPECT_EQ(buffer.availableForRead(), 0u);
}

// ================================================================================

TEST(AudioRingBuffer, DropsSamplesThatDoNotFit) {
    AudioRingBuffer buffer(4);
    const float samples[] = {1.f, 2.f, 3.f, 4.f, 5.f};
    EXPECT_EQ(buffer.write(samples), 4u);
    EXPECT_EQ(buffer.write(std::span<const float>(samples, 1)), 0u);
    float one = 0.f;
    EXPECT_EQ(buffer.read(std::span<float>(&one, 1)), 1u);
    EXPECT_EQ(buffer.write(std::span<const float>(samples, 1)), 1u);
}

// ================================================================================

TEST(AudioRingBuffer, MovesSamplesBetweenTwoThreads) {
    AudioRingBuffer buffer(1024);
    constexpr int total = 20000;
    std::vector<float> source(total);
    for (int i = 0; i < total; ++i) {
        source[static_cast<size_t>(i)] = static_cast<float>(i);
    }
    std::atomic<bool> done{false};
    std::thread producer([&] {
        size_t offset = 0;
        while (offset < source.size()) {
            const size_t wrote = buffer.write(std::span<const float>(source.data() + offset, source.size() - offset));
            offset += wrote;
            if (wrote == 0) {
                std::this_thread::yield();
            }
        }
        done.store(true);
    });

    std::vector<float> received;
    received.reserve(total);
    while (received.size() < static_cast<size_t>(total)) {
        float chunk[128];
        const size_t got = buffer.read(chunk);
        if (got == 0) {
            if (done.load() == true && buffer.availableForRead() == 0) {
                break;
            }
            std::this_thread::yield();
            continue;
        }
        received.insert(received.end(), chunk, chunk + got);
    }
    producer.join();
    ASSERT_EQ(received.size(), source.size());
    EXPECT_FLOAT_EQ(received.front(), 0.f);
    EXPECT_FLOAT_EQ(received.back(), static_cast<float>(total - 1));
}

// ================================================================================

TEST(FftEngine, SinePeaksAtItsBin) {
    constexpr int size = 256;
    constexpr int bin = 16;
    FftEngine engine;
    engine.configure(size);
    const std::vector<float> input = sine(size, bin);
    std::vector<float> levels(static_cast<size_t>(engine.binCount()));
    ASSERT_TRUE(engine.process(input, levels));

    const auto peak = std::max_element(levels.begin(), levels.end());
    EXPECT_EQ(std::distance(levels.begin(), peak), bin);
    EXPECT_GT(*peak, 0.8f);
    EXPECT_LT(levels.front(), 0.2f);
}

// ================================================================================

TEST(Palette, NamedColorsStayInRangeAndUnknownUsesViridis) {
    for (const QString& name : AppSettings::paletteNames()) {
        const QRgb color = Palette::color(name, 0.5f);
        EXPECT_LE(qRed(color), 255);
        EXPECT_LE(qGreen(color), 255);
        EXPECT_LE(qBlue(color), 255);
    }
    EXPECT_EQ(qRed(Palette::color(QStringLiteral("Gray"), 0.f)), 0);
    EXPECT_EQ(qRed(Palette::color(QStringLiteral("Gray"), 1.f)), 255);
    EXPECT_EQ(Palette::color(QStringLiteral("missing"), 0.3f), Palette::color(QStringLiteral("Viridis"), 0.3f));
}

// ================================================================================

TEST(AudioPipeline, SineBecomesAColumnWithLowFrequencyAtTheBottom) {
    audioApp();
    constexpr int size = 256;
    constexpr int bin = 16;
    AudioPipeline pipeline;
    pipeline.prepareOffline(size, QStringLiteral("Viridis"));
    QVector<QRgb> column;
    QObject::connect(&pipeline, &AudioPipeline::columnReady, &pipeline, [&](const QVector<QRgb>& ready) { column = ready; });

    const std::vector<float> input = sine(size, bin);
    EXPECT_EQ(pipeline.writeSamples(input), input.size());
    ASSERT_TRUE(pipeline.processPendingBlock());
    const int bins = size / 2 + 1;
    ASSERT_EQ(column.size(), bins);
    int peak = 0;
    int peakDistance = 0;
    const QRgb quiet = column.at(bins - 1);
    for (int i = 0; i < column.size(); ++i) {
        const int distance = std::abs(qRed(column.at(i)) - qRed(quiet)) + std::abs(qGreen(column.at(i)) - qGreen(quiet))
                             + std::abs(qBlue(column.at(i)) - qBlue(quiet));
        if (distance > peakDistance) {
            peakDistance = distance;
            peak = i;
        }
    }
    EXPECT_EQ(peak, bins - 1 - bin);
}

// ================================================================================

TEST(FrequencyScale, FiveHertzIsTheBottomOfTheLogScale) {
    EXPECT_NEAR(frequencyFromBottom(5.0, 8000.0, true, 5.0), 0.0, 1e-9);
    EXPECT_NEAR(frequencyFromBottom(8000.0, 8000.0, true, 5.0), 1.0, 1e-6);
    const double place = frequencyFromBottom(100.0, 8000.0, true, 5.0);
    EXPECT_GT(place, 0.35);
    EXPECT_LT(place, 0.5);
    EXPECT_NEAR(frequencyFromBottom(100.0, 8000.0, false, 5.0), 100.0 / 8000.0, 1e-9);
}

// ================================================================================

TEST(AudioPipeline, LogScaleLiftsALowTone) {
    audioApp();
    constexpr int size = 256;
    constexpr int bin = 8;
    AudioPipeline pipeline;
    pipeline.prepareOffline(size, QStringLiteral("Viridis"), 0, 16000);
    pipeline.setFrequencyDisplay(true, 5);
    QVector<QRgb> column;
    QObject::connect(&pipeline, &AudioPipeline::columnReady, &pipeline, [&](const QVector<QRgb>& ready) { column = ready; });
    const std::vector<float> input = sine(size, bin);
    EXPECT_EQ(pipeline.writeSamples(input), input.size());
    ASSERT_TRUE(pipeline.processPendingBlock());
    const int bins = size / 2 + 1;
    ASSERT_EQ(column.size(), bins);
    int peak = 0;
    int peakDistance = 0;
    const QRgb quiet = column.back();
    for (int i = 0; i < column.size(); ++i) {
        const int distance = std::abs(qRed(column.at(i)) - qRed(quiet)) + std::abs(qGreen(column.at(i)) - qGreen(quiet)) +
                             std::abs(qBlue(column.at(i)) - qBlue(quiet));
        if (distance > peakDistance) {
            peakDistance = distance;
            peak = i;
        }
    }
    EXPECT_LT(peak, bins / 2);
}

// ================================================================================

TEST(AudioPipeline, HalfOverlapAdvancesByHalfAWindow) {
    audioApp();
    constexpr int size = 256;
    constexpr int bin = 16;
    constexpr int hop = size / 2;
    AudioPipeline pipeline;
    pipeline.prepareOffline(size, QStringLiteral("Viridis"), 50);
    int columns = 0;
    QObject::connect(&pipeline, &AudioPipeline::columnReady, &pipeline, [&](const QVector<QRgb>&) { ++columns; });

    const std::vector<float> tone = sine(size + hop, bin);
    EXPECT_EQ(pipeline.writeSamples(std::span<const float>(tone.data(), static_cast<size_t>(size))),
              static_cast<size_t>(size));
    ASSERT_TRUE(pipeline.processPendingBlock());
    EXPECT_FALSE(pipeline.processPendingBlock());

    EXPECT_EQ(pipeline.writeSamples(std::span<const float>(tone.data() + size, static_cast<size_t>(hop))),
              static_cast<size_t>(hop));
    ASSERT_TRUE(pipeline.processPendingBlock());
    EXPECT_EQ(columns, 2);
    EXPECT_FALSE(pipeline.processPendingBlock());
}

// ================================================================================

TEST(AudioPipeline, FrequencyCeilingDropsBinsAboveTheLimit) {
    audioApp();
    constexpr int size = 256;
    AudioPipeline pipeline;
    pipeline.prepareOffline(size, QStringLiteral("Gray"), 0, 16000, 4000);
    const std::vector<float> input(static_cast<size_t>(size), 0.f);
    EXPECT_EQ(pipeline.writeSamples(input), input.size());
    QVector<QRgb> column;
    QObject::connect(&pipeline, &AudioPipeline::columnReady, &pipeline,
                     [&](const QVector<QRgb>& ready) { column = ready; });
    ASSERT_TRUE(pipeline.processPendingBlock());
    EXPECT_EQ(column.size(), 65);
}

// ================================================================================

TEST(PcmHistory, KeepsOnlyTheMostRecentSamples) {
    PcmHistory history;
    history.reset(8, 1);
    const float input[] = {0.f, 0.5f, -0.5f, 1.f, -1.f, 0.25f, 0.f, 0.f, 0.5f, -1.f};
    history.append(input);
    const std::vector<std::int16_t> kept = history.samples();
    ASSERT_EQ(kept.size(), 8u);
    EXPECT_EQ(kept.front(), static_cast<std::int16_t>(std::lround(-0.5f * 32767.f)));
    EXPECT_EQ(kept.back(), static_cast<std::int16_t>(-32767));
    EXPECT_EQ(history.sampleRate(), 8);
}

// ================================================================================

TEST(PcmHistory, KeepsAbsolutePositionsAfterOldSamplesDrop) {
    PcmHistory history;
    history.reset(8, 1);
    std::vector<float> input(12);
    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<float>(i) / 32767.f;
    }
    history.append(input);
    EXPECT_EQ(history.endPosition(), 12);
    EXPECT_EQ(history.firstPosition(), 4);

    std::vector<float> out(4);
    ASSERT_TRUE(history.readRange(6, out));
    for (size_t i = 0; i < out.size(); ++i) {
        EXPECT_FLOAT_EQ(out[i], input[6 + i]);
    }
    EXPECT_FALSE(history.readRange(3, out));
    EXPECT_FALSE(history.readRange(10, out));
    history.reset(8, 1);
    EXPECT_EQ(history.endPosition(), 0);
    EXPECT_EQ(history.firstPosition(), 0);
}

// ================================================================================

TEST(AudioPipeline, RedrawnColumnsMatchTheLiveOnes) {
    audioApp();
    constexpr int size = 256;
    constexpr int hop = 128;
    AudioPipeline pipeline;
    pipeline.setRecordingSeconds(10);
    pipeline.prepareOffline(size, QStringLiteral("Viridis"), 50, 16000);
    QVector<QVector<QRgb>> live;
    QObject::connect(&pipeline, &AudioPipeline::columnReady, &pipeline,
                     [&](const QVector<QRgb>& ready) { live.push_back(ready); });
    std::vector<float> input(static_cast<size_t>(size * 4));
    for (size_t i = 0; i < input.size(); ++i) {
        // Whole multiples of 1/32767, so the 16-bit history restores them exactly.
        input[i] = std::round(std::sin(0.31 * static_cast<double>(i)) * 9000.0) / 32767.f;
    }
    EXPECT_EQ(pipeline.writeSamples(input), input.size());
    while (pipeline.processPendingBlock() == true) {
    }
    ASSERT_GE(live.size(), 4);
    EXPECT_EQ(pipeline.firstPosition(), 0);
    EXPECT_EQ(pipeline.endPosition(), static_cast<long long>(input.size()));
    for (int k = 0; k < 4; ++k) {
        QVector<QRgb> redrawn;
        ASSERT_TRUE(pipeline.renderColumn(size + static_cast<long long>(k) * hop, size, 0, redrawn)) << k;
        EXPECT_EQ(redrawn, live[k]) << k;
    }
    QVector<QRgb> beyond;
    EXPECT_FALSE(pipeline.renderColumn(static_cast<long long>(input.size()) + 1, size, 0, beyond));
    EXPECT_FALSE(pipeline.renderColumn(size - 1, size, 0, beyond));
}

// ================================================================================

TEST(AudioPipeline, RedrawsWithALongerFftThanTheCapture) {
    audioApp();
    constexpr int size = 256;
    AudioPipeline pipeline;
    pipeline.setRecordingSeconds(10);
    pipeline.prepareOffline(size, QStringLiteral("Viridis"), 50, 16000);
    std::vector<float> input(static_cast<size_t>(size * 8));
    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = std::round(std::sin(0.31 * static_cast<double>(i)) * 9000.0) / 32767.f;
    }
    EXPECT_EQ(pipeline.writeSamples(input), input.size());
    while (pipeline.processPendingBlock() == true) {
    }
    QVector<QRgb> shortColumn;
    QVector<QRgb> longColumn;
    ASSERT_TRUE(pipeline.renderColumn(size * 4, size, 0, shortColumn));
    ASSERT_TRUE(pipeline.renderColumn(size * 4, size * 4, 0, longColumn));
    // Four times the window gives four times the bins over the same frequencies.
    EXPECT_EQ(longColumn.size(), (shortColumn.size() - 1) * 4 + 1);
    EXPECT_NE(longColumn, shortColumn);
    // A window that would start before the first sample cannot be drawn.
    EXPECT_FALSE(pipeline.renderColumn(size * 2, size * 4, 0, longColumn));
}

// ================================================================================

TEST(SpectrumColumn, PoolingKeepsTheStrongestBinOfEachRow) {
    constexpr int bins = 65;
    std::vector<float> levels(bins, 0.f);
    levels[37] = 1.f;
    SpectrumColumnSettings settings;
    settings.fftSize = 128;
    settings.sampleRate = 128;
    settings.palette = QStringLiteral("Gray");

    QVector<QRgb> full;
    buildSpectrumColumn(levels, settings, full);
    ASSERT_EQ(full.size(), bins);

    settings.maxRows = 8;
    QVector<QRgb> pooled;
    buildSpectrumColumn(levels, settings, pooled);
    ASSERT_EQ(pooled.size(), 8);
    int bright = 0;
    for (QRgb color : pooled) {
        bright += qRed(color) == 255 ? 1 : 0;
    }
    EXPECT_EQ(bright, 1);
    // Bin 37 of 65 is 57% of the way up, which is row 3 from the bottom of 8, so row 4 from the top.
    EXPECT_EQ(qRed(pooled[8 - 1 - 4]), 255);

    settings.logarithmic = true;
    settings.maxFrequencyHz = 0;
    settings.logFloorHz = 5;
    buildSpectrumColumn(levels, settings, pooled);
    ASSERT_EQ(pooled.size(), 8);
    bright = 0;
    for (QRgb color : pooled) {
        bright += qRed(color) == 255 ? 1 : 0;
    }
    EXPECT_GE(bright, 1);

    // A limit that is not below the bin count changes nothing.
    settings.logarithmic = false;
    settings.maxRows = bins;
    buildSpectrumColumn(levels, settings, pooled);
    EXPECT_EQ(pooled, full);
}

// ================================================================================

TEST(FftTiming, OverlapCodesAboveSeventyFiveAreExactFractions) {
    EXPECT_EQ(waterfallHop(2048, 0), 2048);
    EXPECT_EQ(waterfallHop(2048, 50), 1024);
    EXPECT_EQ(waterfallHop(2048, 87), 256);
    EXPECT_EQ(waterfallHop(2048, 94), 128);
    EXPECT_EQ(waterfallHop(2048, 97), 64);
    EXPECT_EQ(waterfallHop(2048, 200), 64);
    EXPECT_EQ(waterfallHop(16, 97), 1);
    for (int percent : AppSettings::overlapPercents()) {
        EXPECT_GE(waterfallHop(32768, percent), 1);
    }
}

// ================================================================================

TEST(WavFile, WritesPcm16Mono) {
    audioApp();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("clip.wav"));
    const std::int16_t samples[] = {0, 32767, -32767};
    QString error;
    ASSERT_TRUE(WavFile::write(path, 8000, samples, error));
    EXPECT_TRUE(error.isEmpty());

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    ASSERT_EQ(bytes.size(), 44 + 6);
    EXPECT_EQ(QByteArray(bytes.constData(), 4), QByteArray("RIFF"));
    EXPECT_EQ(QByteArray(bytes.constData() + 8, 4), QByteArray("WAVE"));
    EXPECT_EQ(static_cast<unsigned char>(bytes.at(20)), 1);
    EXPECT_EQ(static_cast<unsigned char>(bytes.at(22)), 1);
    EXPECT_EQ(static_cast<unsigned char>(bytes.at(24)), 8000 & 0xff);
    EXPECT_EQ(static_cast<unsigned char>(bytes.at(44)), 0);
    EXPECT_EQ(static_cast<unsigned char>(bytes.at(45)), 0);
    EXPECT_EQ(static_cast<unsigned char>(bytes.at(46)), 0xff);
    EXPECT_EQ(static_cast<unsigned char>(bytes.at(47)), 0x7f);
    EXPECT_EQ(static_cast<unsigned char>(bytes.at(48)), 0x01);
    EXPECT_EQ(static_cast<unsigned char>(bytes.at(49)), 0x80);
}

// ================================================================================

TEST(AudioPipeline, RecordingSurvivesStopUntilTheNextSession) {
    audioApp();
    AudioPipeline pipeline;
    pipeline.setRecordingSeconds(1);
    pipeline.prepareOffline(256, QStringLiteral("Gray"), 0, 8, 0);
    const float input[] = {0.f, 0.5f, -0.5f, 1.f, -1.f, 0.25f, 0.f, 0.f, 0.5f, -1.f};
    pipeline.writeSamples(input);
    const std::vector<std::int16_t> kept = pipeline.copyRecording();
    ASSERT_EQ(kept.size(), 8u);
    EXPECT_EQ(pipeline.recordingSampleRate(), 8);
    EXPECT_EQ(kept.back(), static_cast<std::int16_t>(-32767));

    pipeline.stop();
    pipeline.setRecordingSeconds(60);
    const std::vector<std::int16_t> afterStop = pipeline.copyRecording();
    ASSERT_EQ(afterStop.size(), 8u);
    EXPECT_EQ(afterStop.back(), static_cast<std::int16_t>(-32767));
    EXPECT_EQ(pipeline.recordingSampleRate(), 8);

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QString error;
    ASSERT_TRUE(pipeline.writeRecording(dir.filePath(QStringLiteral("take.wav")), error));

    pipeline.prepareOffline(256, QStringLiteral("Gray"), 0, 8, 0);
    EXPECT_EQ(pipeline.copyRecording().empty(), true);
}

// ================================================================================

TEST(RecordingSave, ContentUriIsNotOpenedAsAFile) {
    audioApp();
    const QString uri = QStringLiteral("content://com.android.providers.downloads.documents/document/1612.wav");
    EXPECT_EQ(RecordingSave::isContentUri(uri), true);
    EXPECT_EQ(RecordingSave::isContentUri(QStringLiteral("/tmp/1612.wav")), false);

    const std::int16_t sample = 0;
    QString error;
    EXPECT_EQ(RecordingSave::write(uri, RecordingSave::formatById(QStringLiteral("wav")), 8000,
                                   std::span<const std::int16_t>(&sample, 1), error),
              false);
    EXPECT_EQ(error, QStringLiteral("That location cannot be written. Choose a folder the app is allowed to use."));
}

// ================================================================================

TEST(RecordingSave, FileNameUsesTheChosenExtension) {
    audioApp();
    const QList<RecordingFormat> formats = RecordingSave::formats();
    ASSERT_EQ(formats.isEmpty(), false);
    EXPECT_EQ(formats.first().id, QStringLiteral("wav"));
    EXPECT_EQ(formats.first().extension, QStringLiteral("wav"));
    EXPECT_EQ(formats.first().mimeType, QStringLiteral("audio/wav"));

    RecordingFormat compressed;
    compressed.id = QStringLiteral("m4a");
    compressed.extension = QStringLiteral("m4a");
    const QString name =
            RecordingSave::fileName(compressed, QDateTime(QDate(2026, 11, 16), QTime(21, 57, 44)));
    EXPECT_EQ(name, QStringLiteral("waterfall-20261116-215744.m4a"));
    EXPECT_EQ(name.endsWith(QStringLiteral(".wav")), false);

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const std::int16_t samples[] = {0, 1};
    QString error;
    EXPECT_EQ(RecordingSave::write(dir.filePath(QStringLiteral("take.mp3")), compressed, 8000, samples, error), false);
    EXPECT_EQ(error, QStringLiteral("This format is not available."));
}

// ================================================================================

TEST(RecordingSave, WritesWavWithoutForcingAnotherSuffix) {
    audioApp();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("take.wav"));
    const std::int16_t samples[] = {0, 32767};
    QString error;
    ASSERT_EQ(RecordingSave::write(path, RecordingSave::formatById(QStringLiteral("wav")), 8000, samples, error), true);
    EXPECT_EQ(error.isEmpty(), true);
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    EXPECT_EQ(QByteArray(bytes.constData(), 4), QByteArray("RIFF"));
    EXPECT_EQ(static_cast<unsigned char>(bytes.at(46)), 0xff);
    EXPECT_EQ(static_cast<unsigned char>(bytes.at(47)), 0x7f);
}

// ================================================================================

TEST(RecordingSave, WritesMp3OnEveryPlatform) {
    audioApp();
    const QList<RecordingFormat> formats = RecordingSave::formats();
    ASSERT_GE(formats.size(), 2);
    EXPECT_EQ(formats.at(1).id, QStringLiteral("mp3"));
    EXPECT_EQ(formats.at(1).extension, QStringLiteral("mp3"));
    EXPECT_EQ(formats.at(1).mimeType, QStringLiteral("audio/mpeg"));

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    std::vector<std::int16_t> samples(1600);
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<std::int16_t>(8000.0 * std::sin(2.0 * 3.141592653589793 * 440.0 * static_cast<double>(i) / 8000.0));
    }
    const QString path = dir.filePath(QStringLiteral("take.mp3"));
    QString error;
    ASSERT_EQ(RecordingSave::write(path, RecordingSave::formatById(QStringLiteral("mp3")), 8000, samples, error), true);
    EXPECT_EQ(error.isEmpty(), true);
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.read(4);
    ASSERT_GE(bytes.size(), 2);
    const auto first = static_cast<unsigned char>(bytes.at(0));
    const auto second = static_cast<unsigned char>(bytes.at(1));
    const bool frame = first == 0xff && (second & 0xe0) == 0xe0;
    const bool id3 = bytes.startsWith("ID3");
    EXPECT_EQ(frame || id3, true);
}

// ================================================================================

TEST(RecordingSave, RejectsMp3AtAnUnsupportedRate) {
    audioApp();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const std::int16_t sample = 0;
    QString error;
    EXPECT_EQ(RecordingSave::write(dir.filePath(QStringLiteral("high.mp3")), RecordingSave::formatById(QStringLiteral("mp3")),
                                   192000, std::span<const std::int16_t>(&sample, 1), error),
              false);
    EXPECT_EQ(error, QStringLiteral("MP3 cannot be saved at 192000 Hz. Save as WAV instead."));
}

// ================================================================================

TEST(AudioClip, ReadsTheWavItWrites) {
    audioApp();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const std::int16_t samples[] = {0, 1000, -1000, 32767};
    QString error;
    const QString path = dir.filePath(QStringLiteral("tone.wav"));
    ASSERT_TRUE(WavFile::write(path, 8000, samples, error));
    AudioClip clip;
    ASSERT_TRUE(AudioClip::load(path, clip, error));
    EXPECT_EQ(clip.sampleRate, 8000);
    ASSERT_EQ(clip.samples.size(), 4u);
    EXPECT_EQ(clip.samples[1], 1000);
    EXPECT_EQ(clip.samples[2], -1000);
    EXPECT_EQ(clip.samples[3], 32767);
}

// ================================================================================

TEST(AudioClip, ReadsAnMp3ItWrote) {
    audioApp();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    std::vector<std::int16_t> samples(1600);
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<std::int16_t>(8000.0 * std::sin(2.0 * 3.141592653589793 * 440.0 * static_cast<double>(i) / 8000.0));
    }
    QString error;
    const QString path = dir.filePath(QStringLiteral("tone.mp3"));
    ASSERT_TRUE(Mp3File::write(path, 8000, samples, error));
    AudioClip clip;
    ASSERT_TRUE(AudioClip::load(path, clip, error)) << error.toStdString();
    EXPECT_EQ(clip.sampleRate, 8000);
    EXPECT_GT(clip.samples.size(), 0u);
}

// ================================================================================

TEST(AudioPipeline, PlaybackFeedsTheWaterfall) {
    audioApp();
    std::vector<std::int16_t> samples(4000);
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<std::int16_t>(8000.0 * std::sin(2.0 * 3.141592653589793 * 440.0 * static_cast<double>(i) / 8000.0));
    }
    AudioPipeline pipeline;
    QString error;
    ASSERT_TRUE(pipeline.loadClip(samples, 8000, error));
    int columns = 0;
    QObject::connect(&pipeline, &AudioPipeline::columnReady, [&columns](const QVector<QRgb>&) { ++columns; });
    ASSERT_TRUE(pipeline.playClip(QAudioDevice(), 256, 0, 0, QStringLiteral("Gray"), error));
    EXPECT_EQ(pipeline.isPlaying(), true);
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 500 && columns < 1) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    EXPECT_GT(columns, 0);
    pipeline.pauseClip();
    EXPECT_EQ(pipeline.isPlaying(), false);
    EXPECT_EQ(pipeline.clipLoaded(), true);
}
