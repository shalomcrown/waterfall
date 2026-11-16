#include "AppSettings.h"
#include "AudioPipeline.h"
#include "AudioRingBuffer.h"
#include "FftEngine.h"
#include "Palette.h"

#include <QCoreApplication>
#include <QRgb>

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
