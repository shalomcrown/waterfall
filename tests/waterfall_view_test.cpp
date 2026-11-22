#include "AboutDialog.h"
#include "MainWindow.h"
#include "SpectrogramWidget.h"
#include "TimeLabels.h"
#include "ViewTiming.h"
#include "WaterfallView.h"
#include "WavFile.h"

#include <QApplication>
#include <QEventPoint>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QTouchEvent>
#include <QWheelEvent>
#include <QTemporaryDir>
#include <QImage>
#include <QLabel>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>
#include <QRgb>

#include <gtest/gtest.h>

static const bool kOffscreen = [] {
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    return true;
}();

namespace {
QApplication& testApp() {
    static int argc = 1;
    static char arg0[] = "waterfall_tests";
    static char* argv[] = {arg0, nullptr};
    // Intentionally leaked. Destroying QApplication at process exit crashes the offscreen plugin.
    static auto* app = new QApplication(argc, argv);
    return *app;
}

// A sound buffer that is only a pattern: the red value says where a column ends and the
// blue value says which row it is. It counts the columns it was asked to draw.
class PatternSource : public ColumnSource {
public:
    PatternSource(int, long long live) : m_live(live) {}

    bool renderColumn(long long windowEnd, int fftSize, int maxRows, QVector<QRgb>& column) const override {
        ++rendered;
        lastFftSize = fftSize;
        lastMaxRows = maxRows;
        if (windowEnd > m_live || windowEnd - fftSize < m_first) {
            return false;
        }
        column.resize(kRows);
        for (int row = 0; row < kRows; ++row) {
            column[row] = qRgb(static_cast<int>(windowEnd % 251), 0, row);
        }
        return true;
    }
    long long firstPosition() const override { return m_first; }
    long long endPosition() const override { return m_live; }
    long long livePosition() const override { return m_live; }
    void setLive(long long live) { m_live = live; }

    static int redFor(long long windowEnd) { return static_cast<int>(windowEnd % 251); }

    static constexpr int kRows = 100;
    mutable int rendered = 0;
    mutable int lastFftSize = 0;
    mutable int lastMaxRows = 0;

private:
    long long m_live = 0;
    long long m_first = 0;
};

// ================================================================================

WaterfallSession sessionFor(const PatternSource& source, int fftSize, int hop, int rate, bool fromFile = false) {
    WaterfallSession session;
    session.source = &source;
    session.fftSize = fftSize;
    session.hop = hop;
    session.sampleRate = rate;
    session.fromFile = fromFile;
    session.clockOriginLocalMs = (13LL * 3600 + 5 * 60 + 7) * 1000;
    return session;
}
}  // namespace

// ================================================================================

TEST(WaterfallView, StartsAsADarkField) {
    testApp();
    WaterfallView view;
    view.resize(4, 2);
    view.show();
    const QImage& image = view.image();
    ASSERT_EQ(image.width(), 4);
    ASSERT_EQ(image.height(), 2);
    EXPECT_EQ(view.columnsFilled(), 0);
    EXPECT_EQ(image.pixel(0, 0), WaterfallView::background().rgb());
    EXPECT_EQ(image.pixel(3, 1), WaterfallView::background().rgb());
}

// ================================================================================

TEST(WaterfallView, ScrollsNewestFromTheRightEdge) {
    testApp();
    WaterfallView view;
    view.resize(3, 2);
    view.show();

    const QRgb red = qRgb(255, 0, 0);
    const QRgb green = qRgb(0, 255, 0);
    const QRgb blue = qRgb(0, 0, 255);
    const QRgb white = qRgb(255, 255, 255);

    view.appendColumn({red, red});
    EXPECT_EQ(view.columnsFilled(), 1);
    EXPECT_EQ(view.image().pixel(2, 0), red);
    EXPECT_EQ(view.image().pixel(1, 0), WaterfallView::background().rgb());

    view.appendColumn({green, green});
    EXPECT_EQ(view.columnsFilled(), 2);
    EXPECT_EQ(view.image().pixel(2, 0), green);
    EXPECT_EQ(view.image().pixel(1, 0), red);

    view.appendColumn({blue, blue});
    EXPECT_EQ(view.columnsFilled(), 3);
    EXPECT_EQ(view.image().pixel(0, 1), red);
    EXPECT_EQ(view.image().pixel(1, 1), green);
    EXPECT_EQ(view.image().pixel(2, 1), blue);

    view.appendColumn({white, white});
    EXPECT_EQ(view.columnsFilled(), 3);
    EXPECT_EQ(view.image().pixel(0, 0), green);
    EXPECT_EQ(view.image().pixel(1, 0), blue);
    EXPECT_EQ(view.image().pixel(2, 0), white);
}

// ================================================================================

TEST(WaterfallView, ScalesAColumnToTheWidgetHeight) {
    testApp();
    WaterfallView view;
    view.resize(1, 4);
    view.show();
    view.appendColumn({qRgb(0, 0, 0), qRgb(255, 255, 255)});
    EXPECT_EQ(view.image().pixel(0, 0), qRgb(0, 0, 0));
    EXPECT_EQ(view.image().pixel(0, 3), qRgb(255, 255, 255));
}

// ================================================================================

TEST(WaterfallView, ResizeClearsHistory) {
    testApp();
    WaterfallView view;
    view.resize(2, 1);
    view.show();
    view.appendColumn({qRgb(255, 0, 0)});
    view.resize(3, 1);
    EXPECT_EQ(view.columnsFilled(), 0);
    EXPECT_EQ(view.image().pixel(0, 0), WaterfallView::background().rgb());
}

// ================================================================================

TEST(MainWindow, StopStaysDisabledUntilStart) {
    testApp();
    MainWindow window;
    EXPECT_TRUE(window.startButton()->isEnabled());
    EXPECT_FALSE(window.stopButton()->isEnabled());
    EXPECT_EQ(window.startButton()->text(), QStringLiteral("Start"));
    EXPECT_EQ(window.stopButton()->text(), QStringLiteral("Stop"));
    EXPECT_EQ(window.saveButton()->text(), QStringLiteral("Save"));
    EXPECT_EQ(window.loadButton()->text(), QStringLiteral("Load"));
    EXPECT_EQ(window.configureButton()->text(), QStringLiteral("Configure"));
    EXPECT_EQ(window.aboutButton()->text(), QStringLiteral("About"));
    window.startButton()->click();
    if (window.stopButton()->isEnabled() == true) {
        EXPECT_FALSE(window.startButton()->isEnabled());
        window.stopButton()->click();
    }
    EXPECT_TRUE(window.startButton()->isEnabled());
    EXPECT_FALSE(window.stopButton()->isEnabled());
}

// ================================================================================

TEST(MainWindow, LoadSwitchesStartToPlayAndStopToLive) {
    testApp();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const std::int16_t samples[] = {0, 1000, -1000, 500};
    QString error;
    const QString path = dir.filePath(QStringLiteral("tone.wav"));
    ASSERT_TRUE(WavFile::write(path, 8000, samples, error));

    MainWindow window;
    ASSERT_TRUE(window.loadPath(path, error)) << error.toStdString();
    EXPECT_EQ(window.startButton()->text(), QStringLiteral("Play"));
    EXPECT_EQ(window.startButton()->isEnabled(), true);
    EXPECT_EQ(window.stopButton()->text(), QStringLiteral("Live"));
    EXPECT_EQ(window.stopButton()->isEnabled(), true);

    EXPECT_FALSE(window.waterfall()->hasSession());
    window.startButton()->click();
    EXPECT_EQ(window.startButton()->text(), QStringLiteral("Pause"));
    // Playing a file draws from the sound buffer and shows the position in the file.
    EXPECT_TRUE(window.waterfall()->hasSession());
    EXPECT_FALSE(window.waterfall()->timeWindow().clock);
    window.startButton()->click();
    EXPECT_EQ(window.startButton()->text(), QStringLiteral("Play"));
    EXPECT_EQ(window.stopButton()->text(), QStringLiteral("Live"));
}

// ================================================================================

TEST(AboutDialog, ShowsTheReadme) {
    testApp();
    AboutDialog dialog;
    QLabel* version = dialog.findChild<QLabel*>(QStringLiteral("aboutVersion"));
    ASSERT_NE(version, nullptr);
    EXPECT_TRUE(QRegularExpression(QStringLiteral(R"(^0\.1\.0\.\d{8}\.\d{4}\.([0-9a-f]+|unknown)$)"))
                        .match(version->text())
                        .hasMatch())
            << version->text().toStdString();
    auto* layout = qobject_cast<QVBoxLayout*>(dialog.layout());
    ASSERT_NE(layout, nullptr);
    EXPECT_LT(layout->indexOf(version), layout->indexOf(dialog.text()));
    EXPECT_EQ(dialog.findChild<QLabel*>(QStringLiteral("aboutEdition")), nullptr);
    QTextBrowser* browser = dialog.text();
    ASSERT_NE(browser, nullptr);
    const QString plain = browser->toPlainText();
    EXPECT_TRUE(plain.contains(QStringLiteral("Waterfall")));
    EXPECT_TRUE(plain.contains(QStringLiteral("Sample rate")));
    EXPECT_TRUE(plain.contains(QStringLiteral("right edge")));
    const QString html = browser->toHtml();
    EXPECT_TRUE(html.contains(QStringLiteral("<table"), Qt::CaseInsensitive));
    const bool dragSelects = (browser->textInteractionFlags() & Qt::TextSelectableByMouse) != 0;
#if defined(Q_OS_ANDROID)
    EXPECT_FALSE(dragSelects);
#else
    EXPECT_TRUE(dragSelects);
#endif
}

// ================================================================================

TEST(SpectrogramWidget, PaintsFrequencyAndTimeAxes) {
    testApp();
    SpectrogramWidget plot;
    plot.resize(420, 280);
    plot.setScale(1024.0 / 48000.0, 8000.0);
    PatternSource source(4096, 4096 + 1024 * 300);
    plot.beginSession(sessionFor(source, 4096, 1024, 48000));
    plot.show();
    QCoreApplication::processEvents();
    plot.liveAdvanced();
    QWidget* frequency = plot.findChild<QWidget*>(QStringLiteral("frequencyAxis"));
    QWidget* time = plot.findChild<QWidget*>(QStringLiteral("timeAxis"));
    ASSERT_NE(frequency, nullptr);
    ASSERT_NE(time, nullptr);
    plot.waterfall()->resize(300, 200);
    plot.liveAdvanced();

    const auto hasInk = [](const QImage& image) {
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qRed(image.pixel(x, y)) > 40) {
                    return true;
                }
            }
        }
        return false;
    };
    EXPECT_TRUE(hasInk(frequency->grab().toImage()));
    EXPECT_TRUE(hasInk(time->grab().toImage()));
    EXPECT_DOUBLE_EQ(plot.maximumHz(), 8000.0);
}

// ================================================================================

TEST(SpectrogramWidget, TimeLabelsSpanTheFullAxisBeforeItFills) {
    testApp();
    SpectrogramWidget plot;
    plot.resize(480, 320);
    plot.show();
    QCoreApplication::processEvents();
    plot.waterfall()->resize(360, 240);
    QCoreApplication::processEvents();
    plot.setScale(1024.0 / 48000.0, 8000.0);
    PatternSource source(4096, 4096 + 1024 * 300);
    plot.beginSession(sessionFor(source, 4096, 1024, 48000));
    plot.liveAdvanced();

    QWidget* time = plot.findChild<QWidget*>(QStringLiteral("timeAxis"));
    ASSERT_NE(time, nullptr);
    ASSERT_GT(time->width(), 120);
    const QImage image = time->grab().toImage();
    int minX = image.width();
    int maxX = -1;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qRed(image.pixel(x, y)) > 40) {
                if (x < minX) {
                    minX = x;
                }
                if (x > maxX) {
                    maxX = x;
                }
            }
        }
    }
    ASSERT_GE(maxX, 0);
    EXPECT_GT(maxX - minX, image.width() / 2);
}

// ================================================================================

TEST(SpectrogramWidget, AxesDrawTicksBetweenTheLabels) {
    testApp();
    SpectrogramWidget plot;
    plot.resize(480, 320);
    plot.show();
    QCoreApplication::processEvents();
    plot.waterfall()->resize(360, 240);
    QCoreApplication::processEvents();
    plot.setScale(1024.0 / 48000.0, 8000.0);
    PatternSource source(4096, 4096 + 1024 * 300);
    plot.beginSession(sessionFor(source, 4096, 1024, 48000));
    plot.liveAdvanced();

    QWidget* frequency = plot.findChild<QWidget*>(QStringLiteral("frequencyAxis"));
    QWidget* time = plot.findChild<QWidget*>(QStringLiteral("timeAxis"));
    ASSERT_NE(frequency, nullptr);
    ASSERT_NE(time, nullptr);

    const auto countRows = [](const QImage& image, int x0) {
        int rows = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = x0; x < image.width(); ++x) {
                if (qRed(image.pixel(x, y)) > 40) {
                    ++rows;
                    break;
                }
            }
        }
        return rows;
    };
    const auto countColumns = [](const QImage& image, int y1) {
        int columns = 0;
        for (int x = 0; x < image.width(); ++x) {
            for (int y = 0; y < y1 && y < image.height(); ++y) {
                if (qRed(image.pixel(x, y)) > 40) {
                    ++columns;
                    break;
                }
            }
        }
        return columns;
    };
    const QImage frequencyImage = frequency->grab().toImage();
    const QImage timeImage = time->grab().toImage();
    EXPECT_GT(countRows(frequencyImage, frequencyImage.width() - 4), 10);
    EXPECT_GT(countColumns(timeImage, 5), 8);
}

// ================================================================================

TEST(ViewTiming, LevelsGrowTheHopAndThenWidenTheColumns) {
    const LevelGeometry base = levelGeometry(0, 4096, 1024);
    EXPECT_EQ(base.hop, 1024);
    EXPECT_EQ(base.pixelsPerColumn, 1);
    EXPECT_EQ(levelGeometry(2, 4096, 1024).hop, 2048);
    EXPECT_EQ(levelGeometry(-2, 4096, 1024).hop, 512);
    EXPECT_EQ(levelGeometry(-2, 4096, 1024).pixelsPerColumn, 1);
    // The hop stops at an eighth of the window, then the columns get wider.
    EXPECT_EQ(levelGeometry(-4, 4096, 1024).hop, 512);
    EXPECT_EQ(levelGeometry(-4, 4096, 1024).pixelsPerColumn, 2);
    EXPECT_EQ(levelGeometry(-6, 4096, 1024).pixelsPerColumn, 4);
    EXPECT_EQ(minimumZoomLevel(4096, 1024), -10);
    EXPECT_EQ(levelGeometry(-10, 4096, 1024).pixelsPerColumn, kMaxPixelsPerColumn);
}

// ================================================================================

TEST(ViewTiming, ScalingPicksTheNearestLevelWithinTheLimits) {
    EXPECT_EQ(levelAfterScale(0, 2.0, 4096, 1024), -2);
    EXPECT_EQ(levelAfterScale(0, 0.5, 4096, 1024), 2);
    EXPECT_EQ(levelAfterScale(0, 1.0, 4096, 1024), 0);
    EXPECT_EQ(levelAfterScale(0, 1e-9, 4096, 1024), kMaxZoomOutLevel);
    EXPECT_EQ(levelAfterScale(0, 1e9, 4096, 1024), minimumZoomLevel(4096, 1024));
}

// ================================================================================

TEST(ViewTiming, TheGridIsFixedToSamplePositions) {
    EXPECT_EQ(gridEnd(3, 256, 64), 448);
    EXPECT_EQ(gridColumnAtOrBefore(448, 256, 64), 3);
    EXPECT_EQ(gridColumnAtOrBefore(447, 256, 64), 2);
    EXPECT_EQ(gridColumnAtOrBefore(255, 256, 64), -1);
}

// ================================================================================

TEST(ViewTiming, ZoomingInOnFrequencyLengthensTheFft) {
    EXPECT_EQ(effectiveFftSize(2048, 1.0), 2048);
    EXPECT_EQ(effectiveFftSize(2048, 0.5), 4096);
    EXPECT_EQ(effectiveFftSize(2048, 0.25), 8192);
    EXPECT_EQ(effectiveFftSize(2048, 0.02), kMaxRenderFftSize);
    EXPECT_EQ(effectiveFftSize(32768, 0.1), 32768);
    EXPECT_EQ(effectiveFftSize(1, 0.1), 1);
    EXPECT_DOUBLE_EQ(overlapPercentOf(1024, 2048), 50.0);
    EXPECT_DOUBLE_EQ(overlapPercentOf(256, 4096), 93.75);
    EXPECT_DOUBLE_EQ(overlapPercentOf(10, 0), 0.0);
}

// ================================================================================

TEST(WaterfallView, FrequencyZoomDrawsWithALongerFftAndSaysSo) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    EXPECT_EQ(view.effectiveFftSize(), 8);
    EXPECT_DOUBLE_EQ(view.effectiveOverlapPercent(), 50.0);
    // The note is always there, so the FFT size and overlap are known at the default zoom.
    EXPECT_FALSE(view.zoomInfo()->isHidden());
    EXPECT_TRUE(view.zoomInfo()->text().contains(QStringLiteral("FFT 8 ")));
    EXPECT_TRUE(view.zoomInfo()->text().contains(QStringLiteral("overlap 50.0%")));
    EXPECT_TRUE(view.isDefaultZoom());
    EXPECT_FALSE(view.resetButton()->isVisible());
    EXPECT_EQ(source.lastFftSize, 8);

    view.zoomBy(1.0, 4.0, QPointF(0, 4));
    view.commitView();
    EXPECT_EQ(view.effectiveFftSize(), 32);
    EXPECT_EQ(source.lastFftSize, 32);
    EXPECT_DOUBLE_EQ(view.effectiveOverlapPercent(), 87.5);
    EXPECT_FALSE(view.zoomInfo()->isHidden());
    EXPECT_TRUE(view.zoomInfo()->text().contains(QStringLiteral("FFT 32")));
    EXPECT_TRUE(view.zoomInfo()->text().contains(QStringLiteral("31.3 Hz")) ||
                view.zoomInfo()->text().contains(QStringLiteral("31 Hz")));
    EXPECT_TRUE(view.zoomInfo()->text().contains(QStringLiteral("overlap 87.5%")));
    EXPECT_FALSE(view.isDefaultZoom());
    EXPECT_TRUE(view.resetButton()->isVisible());

    // One row per pixel of the part on screen: 4 pixels over a quarter of the axis.
    EXPECT_EQ(source.lastMaxRows, 16);

    view.resetButton()->click();
    EXPECT_EQ(view.effectiveFftSize(), 8);
    EXPECT_EQ(source.lastMaxRows, 4);
    EXPECT_TRUE(view.isDefaultZoom());
    EXPECT_FALSE(view.resetButton()->isVisible());
    EXPECT_FALSE(view.zoomInfo()->isHidden());
    EXPECT_TRUE(view.zoomInfo()->text().contains(QStringLiteral("overlap 50.0%")));
}

// ================================================================================

TEST(WaterfallView, TappingResetReturnsToTheConfiguredZoom) {
    testApp();
    PatternSource source(8, 8 + 4 * 60);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.zoomBy(2.0, 4.0, QPointF(5, 2));
    view.commitView();
    ASSERT_FALSE(view.isDefaultZoom());
    ASSERT_TRUE(view.resetButton()->isVisible());
    const QPointF center = view.resetButton()->geometry().center();
    view.touchPoints({center});
    view.touchPoints({});
    EXPECT_TRUE(view.isDefaultZoom());
    EXPECT_EQ(view.zoomLevel(), 0);
    EXPECT_EQ(view.effectiveFftSize(), 8);
    EXPECT_TRUE(view.following());
}

// ================================================================================

TEST(TimeLabels, StepsAreTidy) {
    EXPECT_DOUBLE_EQ(niceTimeStep(0.0004), 0.001);
    EXPECT_DOUBLE_EQ(niceTimeStep(0.7), 1.0);
    EXPECT_DOUBLE_EQ(niceTimeStep(3.0), 5.0);
    EXPECT_DOUBLE_EQ(niceTimeStep(11.0), 15.0);
    EXPECT_DOUBLE_EQ(niceTimeStep(40.0), 60.0);
    EXPECT_DOUBLE_EQ(niceTimeStep(61.0), 120.0);
}

// ================================================================================

TEST(TimeLabels, ClockShowsHoursMinutesAndSeconds) {
    const long long at = ((13LL * 3600) + 5 * 60 + 7) * 1000 + 250;
    EXPECT_EQ(formatClockTime(at, 1.0), QStringLiteral("13:05:07"));
    EXPECT_EQ(formatClockTime(at, 5.0), QStringLiteral("13:05:07"));
    EXPECT_EQ(formatClockTime(at, 60.0), QStringLiteral("13:05"));
    EXPECT_EQ(formatClockTime(at, 0.1), QStringLiteral("13:05:07.2"));
    EXPECT_EQ(formatClockTime(at, 0.01), QStringLiteral("13:05:07.250"));
    EXPECT_EQ(formatClockTime(-1000, 1.0), QStringLiteral("23:59:59"));
}

// ================================================================================

TEST(TimeLabels, FilePositionShowsMinutesAndSeconds) {
    EXPECT_EQ(formatPosition(65.0, 1.0), QStringLiteral("01:05"));
    EXPECT_EQ(formatPosition(3725.0, 1.0), QStringLiteral("1:02:05"));
    EXPECT_EQ(formatPosition(1.5, 0.5), QStringLiteral("00:01.5"));
    EXPECT_EQ(formatPosition(1.25, 0.001), QStringLiteral("00:01.250"));
    EXPECT_EQ(formatPosition(-3.0, 1.0), QStringLiteral("00:00"));
}

// ================================================================================

TEST(WaterfallView, DrawsFromTheSoundBufferAndFollowsTheLiveEdge) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    EXPECT_TRUE(view.following());
    EXPECT_EQ(view.columnsFilled(), 10);
    EXPECT_EQ(qRed(view.image().pixel(9, 0)), PatternSource::redFor(8 + 4 * 30));
    EXPECT_EQ(qRed(view.image().pixel(0, 0)), PatternSource::redFor(8 + 4 * 21));
    EXPECT_FALSE(view.latestButton()->isVisible());

    const WaterfallTimeWindow window = view.timeWindow();
    EXPECT_TRUE(window.valid);
    EXPECT_TRUE(window.clock);
    EXPECT_NEAR(window.rightSeconds, (8.0 + 4 * 30) / 1000.0, 1e-9);
    EXPECT_NEAR(window.rightSeconds - window.leftSeconds, 10 * 4 / 1000.0, 1e-9);
    EXPECT_EQ(window.originLocalMs, (13LL * 3600 + 5 * 60 + 7) * 1000);
}

// ================================================================================

TEST(WaterfallView, AFileShowsThePositionInsteadOfTheClock) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000, true));
    EXPECT_FALSE(view.timeWindow().clock);
}

// ================================================================================

TEST(WaterfallView, PanningDrawsOnlyTheUncoveredStrip) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    const int before = source.rendered;

    view.panBy(3.0, 0.0);
    EXPECT_FALSE(view.following());
    EXPECT_TRUE(view.latestButton()->isVisible());
    EXPECT_EQ(source.rendered - before, 3);
    EXPECT_EQ(qRed(view.image().pixel(9, 0)), PatternSource::redFor(8 + 4 * 27));
    EXPECT_EQ(qRed(view.image().pixel(0, 0)), PatternSource::redFor(8 + 4 * 18));
    EXPECT_EQ(view.columnsFilled(), 10);
}

// ================================================================================

TEST(WaterfallView, APinnedViewStaysPutWhileTheSoundKeepsComing) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.panBy(3.0, 0.0);
    const double right = view.timeWindow().rightSeconds;

    source.setLive(8 + 4 * 35);
    view.liveAdvanced();
    EXPECT_FALSE(view.following());
    EXPECT_EQ(qRed(view.image().pixel(9, 0)), PatternSource::redFor(8 + 4 * 27));
    EXPECT_DOUBLE_EQ(view.timeWindow().rightSeconds, right);

    view.panBy(-3.0, 0.0);
    EXPECT_FALSE(view.following());
    view.panBy(-100.0, 0.0);
    EXPECT_TRUE(view.following());
    EXPECT_FALSE(view.latestButton()->isVisible());
    EXPECT_EQ(qRed(view.image().pixel(9, 0)), PatternSource::redFor(8 + 4 * 35));
}

// ================================================================================

TEST(WaterfallView, PanningStopsAtTheOldestSound) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.panBy(10000.0, 0.0);
    EXPECT_EQ(qRed(view.image().pixel(0, 0)), PatternSource::redFor(8));
    EXPECT_EQ(qRed(view.image().pixel(9, 0)), PatternSource::redFor(8 + 4 * 9));
}

// ================================================================================

TEST(WaterfallView, LatestButtonReturnsToTheLiveEdge) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.panBy(5.0, 0.0);
    ASSERT_TRUE(view.latestButton()->isVisible());
    view.latestButton()->click();
    EXPECT_TRUE(view.following());
    EXPECT_EQ(qRed(view.image().pixel(9, 0)), PatternSource::redFor(8 + 4 * 30));
}

// ================================================================================

TEST(WaterfallView, ZoomingInTimeKeepsTheLiveEdgeAndShowsLessTime) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();

    view.zoomBy(2.0, 1.0, QPointF(10, 0));
    EXPECT_TRUE(view.previewActive());
    EXPECT_EQ(view.zoomLevel(), 0);
    view.commitView();
    EXPECT_FALSE(view.previewActive());
    EXPECT_EQ(view.zoomLevel(), -2);
    EXPECT_TRUE(view.following());
    EXPECT_EQ(qRed(view.image().pixel(9, 0)), PatternSource::redFor(8 + 4 * 30));
    EXPECT_EQ(qRed(view.image().pixel(8, 0)), PatternSource::redFor(8 + 4 * 30 - 2));
    const WaterfallTimeWindow window = view.timeWindow();
    EXPECT_NEAR(window.rightSeconds - window.leftSeconds, 10 * 2 / 1000.0, 1e-9);
}

// ================================================================================

TEST(WaterfallView, ZoomingAroundAMiddlePointPinsTheView) {
    testApp();
    PatternSource source(8, 8 + 4 * 200);
    WaterfallView view;
    view.resize(100, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    const double middle = (view.timeWindow().leftSeconds + view.timeWindow().rightSeconds) / 2.0;

    view.zoomBy(2.0, 1.0, QPointF(50, 0));
    view.commitView();
    EXPECT_FALSE(view.following());
    EXPECT_TRUE(view.latestButton()->isVisible());
    const WaterfallTimeWindow window = view.timeWindow();
    EXPECT_NEAR((window.leftSeconds + window.rightSeconds) / 2.0, middle, 0.004);
    EXPECT_NEAR(window.rightSeconds - window.leftSeconds, 100 * 2 / 1000.0, 1e-9);
}

// ================================================================================

TEST(WaterfallView, TimeZoomStopsAtBothEnds) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    for (int i = 0; i < 40; ++i) {
        view.zoomBy(0.5, 1.0, QPointF(10, 0));
    }
    view.commitView();
    EXPECT_EQ(view.zoomLevel(), kMaxZoomOutLevel);
    view.resetView();
    EXPECT_EQ(view.zoomLevel(), 0);
    for (int i = 0; i < 40; ++i) {
        view.zoomBy(2.0, 1.0, QPointF(10, 0));
    }
    view.commitView();
    EXPECT_EQ(view.zoomLevel(), minimumZoomLevel(8, 4));
    EXPECT_EQ(view.columnPixels(), kMaxPixelsPerColumn);
}

// ================================================================================

TEST(WaterfallView, FrequencyZoomShowsPartOfTheAxis) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    EXPECT_EQ(qBlue(view.image().pixel(9, 0)), 0);
    EXPECT_EQ(qBlue(view.image().pixel(9, 3)), 75);

    view.zoomBy(1.0, 2.0, QPointF(0, 4));
    view.commitView();
    WaterfallFrequencyWindow window = view.frequencyWindow();
    EXPECT_NEAR(window.low, 0.0, 1e-9);
    EXPECT_NEAR(window.high, 0.5, 1e-9);
    EXPECT_EQ(view.zoomLevel(), 0);
    EXPECT_EQ(qBlue(view.image().pixel(9, 0)), 50);
    EXPECT_EQ(qBlue(view.image().pixel(9, 3)), 87);

    view.previewPanBy(0.0, 2.0);
    view.commitView();
    window = view.frequencyWindow();
    EXPECT_NEAR(window.low, 0.25, 1e-9);
    EXPECT_NEAR(window.high, 0.75, 1e-9);
    EXPECT_EQ(qBlue(view.image().pixel(9, 0)), 25);

    // It cannot be pushed off the axis.
    view.previewPanBy(0.0, 1000.0);
    view.commitView();
    EXPECT_NEAR(view.frequencyWindow().high, 1.0, 1e-9);
    EXPECT_NEAR(view.frequencyWindow().low, 0.5, 1e-9);
}

// ================================================================================

TEST(WaterfallView, FrequencyZoomCannotGoBeyondTheWholeAxis) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 40);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.zoomBy(1.0, 0.1, QPointF(0, 20));
    view.commitView();
    EXPECT_NEAR(view.frequencyWindow().low, 0.0, 1e-9);
    EXPECT_NEAR(view.frequencyWindow().high, 1.0, 1e-9);
    view.zoomBy(1.0, 1000.0, QPointF(0, 20));
    view.commitView();
    EXPECT_NEAR(view.frequencyWindow().high - view.frequencyWindow().low, kMinFrequencySpan, 1e-9);
}

// ================================================================================

TEST(WaterfallView, ResizeRedrawsFromTheSoundBuffer) {
    testApp();
    PatternSource source(8, 8 + 4 * 60);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.resize(20, 4);
    EXPECT_EQ(view.image().width(), 20);
    EXPECT_EQ(view.columnsFilled(), 20);
    EXPECT_EQ(qRed(view.image().pixel(19, 0)), PatternSource::redFor(8 + 4 * 60));
    EXPECT_EQ(qRed(view.image().pixel(0, 0)), PatternSource::redFor(8 + 4 * 41));
}

// ================================================================================

TEST(WaterfallView, RedrawDrawsEverythingAgain) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    const int before = source.rendered;
    view.redraw();
    EXPECT_EQ(source.rendered - before, 10);
    EXPECT_EQ(view.columnsFilled(), 10);
}

// ================================================================================

TEST(WaterfallView, AFreshSessionStartsAtTheLiveEdgeAtNormalZoom) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.zoomBy(2.0, 2.0, QPointF(5, 2));
    view.commitView();
    view.panBy(4.0, 0.0);
    ASSERT_NE(view.zoomLevel(), 0);
    view.beginSession(sessionFor(source, 8, 4, 1000));
    EXPECT_EQ(view.zoomLevel(), 0);
    EXPECT_TRUE(view.following());
    EXPECT_EQ(view.columnsFilled(), 0);
    EXPECT_DOUBLE_EQ(view.frequencyWindow().low, 0.0);
    EXPECT_DOUBLE_EQ(view.frequencyWindow().high, 1.0);
}

// ================================================================================

TEST(WaterfallView, ResetViewGoesBackToNormalZoomAtTheLiveEdge) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.zoomBy(2.0, 2.0, QPointF(5, 2));
    view.commitView();
    view.panBy(3.0, 0.0);
    view.resetView();
    EXPECT_EQ(view.zoomLevel(), 0);
    EXPECT_TRUE(view.following());
    EXPECT_FALSE(view.latestButton()->isVisible());
    EXPECT_DOUBLE_EQ(view.frequencyWindow().high, 1.0);
    EXPECT_EQ(qRed(view.image().pixel(9, 0)), PatternSource::redFor(8 + 4 * 30));
}

// ================================================================================

TEST(WaterfallView, ADragWithTheMouseMovesTheView) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();

    QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 2), QPointF(5, 2), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&view, &press);
    QMouseEvent move(QEvent::MouseMove, QPointF(8, 2), QPointF(8, 2), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(8, 2), QPointF(8, 2), Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(&view, &release);
    EXPECT_FALSE(view.following());
    EXPECT_EQ(qRed(view.image().pixel(9, 0)), PatternSource::redFor(8 + 4 * 27));

    QMouseEvent doubleClick(QEvent::MouseButtonDblClick, QPointF(5, 2), QPointF(5, 2), Qt::LeftButton,
                            Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &doubleClick);
    EXPECT_TRUE(view.following());
    EXPECT_EQ(qRed(view.image().pixel(9, 0)), PatternSource::redFor(8 + 4 * 30));
}

// ================================================================================

TEST(WaterfallView, ControlWheelZoomsTimeAndControlShiftWheelZoomsFrequency) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 40);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();

    QWheelEvent time(QPointF(10, 20), QPointF(10, 20), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::ControlModifier,
                     Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &time);
    EXPECT_TRUE(view.previewActive());
    view.commitView();
    EXPECT_EQ(view.zoomLevel(), -1);
    EXPECT_DOUBLE_EQ(view.frequencyWindow().high - view.frequencyWindow().low, 1.0);

    QWheelEvent frequency(QPointF(5, 40), QPointF(5, 40), QPoint(), QPoint(0, 240), Qt::NoButton,
                          Qt::ControlModifier | Qt::ShiftModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &frequency);
    view.commitView();
    EXPECT_EQ(view.zoomLevel(), -1);
    EXPECT_LT(view.frequencyWindow().high - view.frequencyWindow().low, 0.7);
}

// ================================================================================

TEST(WaterfallView, PlainWheelPansInTime) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    QWheelEvent wheel(QPointF(5, 2), QPointF(5, 2), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &wheel);
    EXPECT_FALSE(view.following());
    EXPECT_EQ(qRed(view.image().pixel(9, 0)), PatternSource::redFor(8 + 4 * 29));
}

// ================================================================================

TEST(WaterfallView, KeysZoomPanAndReturn) {
    testApp();
    PatternSource source(8, 8 + 4 * 100);
    WaterfallView view;
    view.resize(10, 40);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    const auto press = [&](int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QKeyEvent event(QEvent::KeyPress, key, modifiers);
        QApplication::sendEvent(&view, &event);
    };
    press(Qt::Key_Plus);
    EXPECT_EQ(view.zoomLevel(), -1);
    press(Qt::Key_Minus);
    press(Qt::Key_Minus);
    EXPECT_EQ(view.zoomLevel(), 1);
    press(Qt::Key_Left);
    EXPECT_FALSE(view.following());
    press(Qt::Key_End);
    EXPECT_TRUE(view.following());
    press(Qt::Key_Plus, Qt::ShiftModifier);
    EXPECT_LT(view.frequencyWindow().high - view.frequencyWindow().low, 1.0);
    press(Qt::Key_Home);
    EXPECT_EQ(view.zoomLevel(), 0);
    EXPECT_DOUBLE_EQ(view.frequencyWindow().high - view.frequencyWindow().low, 1.0);
}

// ================================================================================

TEST(WaterfallView, OneFingerPans) {
    testApp();
    PatternSource source(8, 8 + 4 * 30);
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.touchPoints({QPointF(5, 2)});
    view.touchPoints({QPointF(8, 2)});
    view.touchPoints({});
    EXPECT_FALSE(view.following());
    EXPECT_EQ(qRed(view.image().pixel(9, 0)), PatternSource::redFor(8 + 4 * 27));
}

// ================================================================================

TEST(WaterfallView, ASidewaysPinchZoomsTimeOnly) {
    testApp();
    PatternSource source(8, 8 + 4 * 300);
    WaterfallView view;
    view.resize(100, 40);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.touchPoints({QPointF(40, 20), QPointF(60, 20)});
    view.touchPoints({QPointF(30, 20), QPointF(70, 20)});
    EXPECT_TRUE(view.previewActive());
    view.touchPoints({});
    EXPECT_EQ(view.zoomLevel(), -2);
    EXPECT_DOUBLE_EQ(view.frequencyWindow().high - view.frequencyWindow().low, 1.0);
}

// ================================================================================

TEST(WaterfallView, AnUpAndDownPinchZoomsFrequencyOnly) {
    testApp();
    PatternSource source(8, 8 + 4 * 300);
    WaterfallView view;
    view.resize(100, 40);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.touchPoints({QPointF(50, 10), QPointF(50, 30)});
    view.touchPoints({QPointF(50, 5), QPointF(50, 35)});
    view.touchPoints({});
    EXPECT_EQ(view.zoomLevel(), 0);
    EXPECT_NEAR(view.frequencyWindow().high - view.frequencyWindow().low, 1.0 / 1.5, 1e-6);
}

// ================================================================================

TEST(WaterfallView, ADiagonalPinchZoomsBoth) {
    testApp();
    PatternSource source(8, 8 + 4 * 300);
    WaterfallView view;
    view.resize(100, 40);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.touchPoints({QPointF(40, 15), QPointF(60, 35)});
    view.touchPoints({QPointF(30, 5), QPointF(70, 45)});
    view.touchPoints({});
    EXPECT_NE(view.zoomLevel(), 0);
    EXPECT_LT(view.frequencyWindow().high - view.frequencyWindow().low, 1.0);
}

// ================================================================================

TEST(WaterfallView, TappingLatestWithAFingerReturnsToTheLiveEdge) {
    testApp();
    PatternSource source(8, 8 + 4 * 300);
    WaterfallView view;
    view.resize(100, 40);
    view.show();
    view.beginSession(sessionFor(source, 8, 4, 1000));
    view.liveAdvanced();
    view.panBy(10.0, 0.0);
    ASSERT_FALSE(view.following());
    ASSERT_TRUE(view.latestButton()->isVisible());
    const QPointF center = view.latestButton()->geometry().center();
    view.touchPoints({center});
    view.touchPoints({});
    EXPECT_TRUE(view.following());
}

// ================================================================================

TEST(WaterfallView, ZoomAndPanDoNothingWithoutASession) {
    testApp();
    WaterfallView view;
    view.resize(10, 4);
    view.show();
    view.panBy(3.0, 3.0);
    view.zoomBy(2.0, 2.0, QPointF(5, 2));
    view.commitView();
    EXPECT_FALSE(view.previewActive());
    EXPECT_FALSE(view.timeWindow().valid);
    EXPECT_FALSE(view.latestButton()->isVisible());
}

// ================================================================================

TEST(SpectrogramWidget, FrequencyAxisFollowsTheZoomedWindow) {
    testApp();
    SpectrogramWidget plot;
    plot.resize(480, 320);
    plot.setScale(1024.0 / 48000.0, 8000.0);
    PatternSource source(4096, 4096 + 1024 * 300);
    plot.beginSession(sessionFor(source, 4096, 1024, 48000));
    plot.show();
    QCoreApplication::processEvents();
    plot.liveAdvanced();
    QWidget* frequency = plot.findChild<QWidget*>(QStringLiteral("frequencyAxis"));
    ASSERT_NE(frequency, nullptr);
    const QImage whole = frequency->grab().toImage();
    plot.waterfall()->zoomBy(1.0, 8.0, QPointF(0, plot.waterfall()->height()));
    plot.waterfall()->commitView();
    EXPECT_LT(plot.waterfall()->frequencyWindow().high, 0.2);
    const QImage zoomed = frequency->grab().toImage();
    EXPECT_NE(whole, zoomed);
}

// ================================================================================

TEST(SpectrogramWidget, TimeAxisFollowsTheView) {
    testApp();
    SpectrogramWidget plot;
    plot.resize(480, 320);
    plot.setScale(1024.0 / 48000.0, 8000.0);
    PatternSource source(4096, 4096 + 1024 * 3000);
    plot.beginSession(sessionFor(source, 4096, 1024, 48000));
    plot.show();
    QCoreApplication::processEvents();
    plot.liveAdvanced();
    QWidget* time = plot.findChild<QWidget*>(QStringLiteral("timeAxis"));
    ASSERT_NE(time, nullptr);
    const QImage live = time->grab().toImage();
    plot.waterfall()->panBy(60.0, 0.0);
    const QImage panned = time->grab().toImage();
    EXPECT_NE(live, panned);
}
