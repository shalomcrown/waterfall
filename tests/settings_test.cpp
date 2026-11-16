#include "AppSettings.h"
#include "AudioPermission.h"
#include "SettingsDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {
QApplication& settingsApp() {
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    if (auto* existing = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        return *existing;
    }
    static int argc = 1;
    static char arg0[] = "waterfall_tests";
    static char* argv[] = {arg0, nullptr};
    static auto* app = new QApplication(argc, argv);
    return *app;
}

// ================================================================================

void useTemporarySettings(QTemporaryDir& dir) {
    QCoreApplication::setOrganizationName(QStringLiteral("waterfall"));
    QCoreApplication::setApplicationName(QStringLiteral("waterfall"));
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, dir.path());
    QSettings settings;
    settings.clear();
    settings.sync();
}
}  // namespace

// ================================================================================

TEST(AppSettings, DefaultsAndImmediateRoundTrip) {
    settingsApp();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    useTemporarySettings(dir);

    EXPECT_EQ(AppSettings::sampleRate(), 48000);
    EXPECT_EQ(AppSettings::fftSize(), 2048);
    EXPECT_EQ(AppSettings::overlapPercent(), 50);
    EXPECT_EQ(AppSettings::maxFrequencyHz(), 8000);
    EXPECT_FALSE(AppSettings::keepScreenOn());
    EXPECT_EQ(AppSettings::palette(), QStringLiteral("Viridis"));
    EXPECT_EQ(AppSettings::fftSizes(), (QList<int>{1024, 2048, 4096}));

    AppSettings::setInputDeviceId(QStringLiteral("mic-1"));
    AppSettings::setOutputDeviceId(QStringLiteral("spk-1"));
    AppSettings::setSampleRate(44100);
    AppSettings::setFftSize(4096);
    AppSettings::setOverlapPercent(75);
    AppSettings::setMaxFrequencyHz(4000);
    AppSettings::setKeepScreenOn(true);
    AppSettings::setPalette(QStringLiteral("Magma"));

    EXPECT_EQ(AppSettings::inputDeviceId(), QStringLiteral("mic-1"));
    EXPECT_EQ(AppSettings::outputDeviceId(), QStringLiteral("spk-1"));
    EXPECT_EQ(AppSettings::sampleRate(), 44100);
    EXPECT_EQ(AppSettings::fftSize(), 4096);
    EXPECT_EQ(AppSettings::overlapPercent(), 75);
    EXPECT_EQ(AppSettings::maxFrequencyHz(), 4000);
    EXPECT_TRUE(AppSettings::keepScreenOn());
    EXPECT_EQ(AppSettings::palette(), QStringLiteral("Magma"));
}

// ================================================================================

TEST(SettingsDialog, ChangingAPulldownSavesImmediately) {
    settingsApp();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    useTemporarySettings(dir);

    SettingsDialog dialog;
    QComboBox* fft = dialog.fftCombo();
    QComboBox* palette = dialog.paletteCombo();
    ASSERT_NE(fft, nullptr);
    ASSERT_NE(palette, nullptr);

    const QStringList names = AppSettings::paletteNames();
    for (const QString& name : {QStringLiteral("Viridis"), QStringLiteral("Magma"), QStringLiteral("Inferno"),
                                QStringLiteral("Jet")}) {
        EXPECT_GE(palette->findData(name), 0) << name.toStdString();
    }
    EXPECT_EQ(names.size(), 9);

    fft->setCurrentIndex(fft->findData(1024));
    EXPECT_EQ(AppSettings::fftSize(), 1024);
    palette->setCurrentIndex(palette->findData(QStringLiteral("Hot")));
    EXPECT_EQ(AppSettings::palette(), QStringLiteral("Hot"));
    QComboBox* overlap = dialog.overlapCombo();
    ASSERT_NE(overlap, nullptr);
    EXPECT_EQ(overlap->currentData().toInt(), 50);
    overlap->setCurrentIndex(overlap->findData(75));
    EXPECT_EQ(AppSettings::overlapPercent(), 75);
    QComboBox* range = dialog.maxFrequencyCombo();
    ASSERT_NE(range, nullptr);
    EXPECT_EQ(range->currentData().toInt(), 8000);
    range->setCurrentIndex(range->findData(0));
    EXPECT_EQ(AppSettings::maxFrequencyHz(), 0);
    ASSERT_NE(dialog.keepScreenCheck(), nullptr);
    dialog.keepScreenCheck()->setChecked(true);
    EXPECT_TRUE(AppSettings::keepScreenOn());
    QComboBox* rates = dialog.sampleRateCombo();
    if (rates->count() > 1) {
        const int other = rates->currentIndex() == 0 ? 1 : 0;
        rates->setCurrentIndex(other);
        EXPECT_EQ(AppSettings::sampleRate(), rates->currentData().toInt());
    }
}

// ================================================================================

TEST(SettingsDialog, CloseButtonDismissesTheDialog) {
    settingsApp();
    SettingsDialog dialog;
    QPushButton* close = dialog.closeButton();
    ASSERT_NE(close, nullptr);
    EXPECT_EQ(close->text(), QStringLiteral("Close"));
    EXPECT_EQ(close->accessibleName(), QStringLiteral("Close"));
    EXPECT_GE(close->minimumHeight(), 44);

    dialog.show();
    close->click();
    EXPECT_EQ(dialog.result(), QDialog::Rejected);
    EXPECT_FALSE(dialog.isVisible());
}

// ================================================================================

TEST(AudioPermission, InstructionsMentionTheMicrophone) {
    EXPECT_TRUE(AudioPermission::instructions().contains(QStringLiteral("microphone"), Qt::CaseInsensitive));
}
