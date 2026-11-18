#include "AboutDialog.h"
#include "MainWindow.h"
#include "SpectrogramWidget.h"
#include "WaterfallView.h"
#include "WavFile.h"

#include <QApplication>
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

    window.startButton()->click();
    EXPECT_EQ(window.startButton()->text(), QStringLiteral("Pause"));
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
    plot.show();
    QCoreApplication::processEvents();
    QWidget* frequency = plot.findChild<QWidget*>(QStringLiteral("frequencyAxis"));
    QWidget* time = plot.findChild<QWidget*>(QStringLiteral("timeAxis"));
    ASSERT_NE(frequency, nullptr);
    ASSERT_NE(time, nullptr);
    plot.waterfall()->resize(300, 200);
    const QRgb sample = qRgb(253, 231, 37);
    const QRgb bins[] = {sample, sample, sample, sample};
    plot.appendColumn(bins);

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
    const QRgb sample = qRgb(253, 231, 37);
    const QRgb bins[] = {sample, sample, sample, sample};
    plot.appendColumn(bins);

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
    const QRgb sample = qRgb(253, 231, 37);
    const QRgb bins[] = {sample, sample, sample, sample};
    plot.appendColumn(bins);

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
