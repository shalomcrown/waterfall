#include "MainWindow.h"
#include "SpectrogramWidget.h"
#include "WaterfallView.h"

#include <QApplication>
#include <QImage>
#include <QPushButton>
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
    window.startButton()->click();
    if (window.stopButton()->isEnabled() == true) {
        EXPECT_FALSE(window.startButton()->isEnabled());
        window.stopButton()->click();
    }
    EXPECT_TRUE(window.startButton()->isEnabled());
    EXPECT_FALSE(window.stopButton()->isEnabled());
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
