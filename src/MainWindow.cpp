#include "MainWindow.h"

#include "AppIcon.h"
#include "AppSettings.h"
#include "AudioPermission.h"
#include "AudioPipeline.h"
#include "FftTiming.h"
#include "ScreenAwake.h"
#include "SettingsDialog.h"
#include "SpectrogramWidget.h"
#include "WaterfallView.h"

#include <QApplication>
#include <QGuiApplication>
#include <QShowEvent>
#include <QAudioDevice>
#include <QIcon>
#include <QHBoxLayout>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <span>

namespace {
QIcon themedIcon(QStyle* style, const char* themeName, QStyle::StandardPixmap fallback) {
    return QIcon::fromTheme(QString::fromUtf8(themeName), style->standardIcon(fallback));
}

// ================================================================================

QPushButton* makeButton(QWidget* parent, const QString& text, const QIcon& icon) {
    auto* button = new QPushButton(icon, text, parent);
    button->setToolTip(text);
    button->setMinimumHeight(44);
    return button;
}

// ================================================================================

QAudioDevice selectedInputDevice() {
    const QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
    const QString saved = AppSettings::inputDeviceId();
    if (saved.isEmpty() == false) {
        for (const QAudioDevice& device : inputs) {
            if (QString::fromUtf8(device.id()) == saved) {
                return device;
            }
        }
    }
    return QMediaDevices::defaultAudioInput();
}

// ================================================================================

void execMessage(QMessageBox& box) {
    if (QApplication::platformName() == QStringLiteral("offscreen")) {
        QTimer::singleShot(0, &box, &QDialog::reject);
    }
    box.exec();
}
}  // namespace

// ================================================================================

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Waterfall"));
    AppIcon::apply(this);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    m_spectrogram = new SpectrogramWidget(central);
    m_waterfall = m_spectrogram->waterfall();
    layout->addWidget(m_spectrogram, 1);

    auto* controls = new QHBoxLayout();
    controls->setSpacing(8);
    QStyle* style = this->style();
    m_start = makeButton(central, QStringLiteral("Start"),
                         themedIcon(style, "media-playback-start", QStyle::SP_MediaPlay));
    m_stop = makeButton(central, QStringLiteral("Stop"),
                        themedIcon(style, "media-playback-stop", QStyle::SP_MediaStop));
    m_save = makeButton(central, QStringLiteral("Save"),
                        themedIcon(style, "document-save", QStyle::SP_DialogSaveButton));
    m_load = makeButton(central, QStringLiteral("Load"),
                        themedIcon(style, "document-open", QStyle::SP_DialogOpenButton));
    m_configure = makeButton(central, QStringLiteral("Configure"),
                             themedIcon(style, "preferences-system", QStyle::SP_FileDialogDetailedView));
    controls->addWidget(m_start);
    controls->addWidget(m_stop);
    controls->addWidget(m_save);
    controls->addWidget(m_load);
    controls->addWidget(m_configure);
    controls->addStretch(1);
    layout->addLayout(controls);

    setCentralWidget(central);
    resize(800, 480);
    setRunning(false);

    m_pipeline = new AudioPipeline(this);
    connect(m_pipeline, &AudioPipeline::columnReady, this, [this](const QVector<QRgb>& column) {
        if (m_spectrogram == nullptr || column.isEmpty() == true) {
            return;
        }
        m_spectrogram->appendColumn(std::span<const QRgb>(column.constData(), static_cast<size_t>(column.size())));
    });
    refreshScale();
    applyKeepAwake();
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            applyKeepAwake();
        }
    });
    QTimer::singleShot(800, this, &MainWindow::applyKeepAwake);
    QTimer::singleShot(2000, this, &MainWindow::applyKeepAwake);

    connect(m_start, &QPushButton::clicked, this, &MainWindow::startCapture);
    connect(m_stop, &QPushButton::clicked, this, &MainWindow::stopCapture);
    connect(m_configure, &QPushButton::clicked, this, &MainWindow::openSettings);
    QTimer::singleShot(0, this, &MainWindow::ensureMicrophonePermission);
}

// ================================================================================

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    applyKeepAwake();
}

// ================================================================================

void MainWindow::applyKeepAwake() {
    ScreenAwake::setKeepAwake(AppSettings::keepScreenOn());
}

// ================================================================================

void MainWindow::setRunning(bool running) {
    m_start->setEnabled(running == false);
    m_stop->setEnabled(running);
}

// ================================================================================

void MainWindow::startCapture() {
    const AudioPermission::Status current = AudioPermission::status();
    if (current == AudioPermission::Status::Undetermined) {
        AudioPermission::request([this](AudioPermission::Status result) {
            if (result != AudioPermission::Status::Granted) {
                showPermissionDialog();
                return;
            }
            startCapture();
        });
        return;
    }
    if (current != AudioPermission::Status::Granted) {
        showPermissionDialog();
        return;
    }

    QString error;
    const bool started =
            m_pipeline->start(selectedInputDevice(), AppSettings::sampleRate(), AppSettings::fftSize(),
                              AppSettings::overlapPercent(), AppSettings::maxFrequencyHz(), AppSettings::palette(), error);
    if (started == false) {
        setRunning(false);
        if (error.isEmpty() == true) {
            error = tr("Audio capture did not start.");
        }
        showAudioError(error);
        return;
    }
    const int rate = m_pipeline->captureSampleRate();
    if (rate > 1 && m_pipeline->hopSamples() > 0) {
        m_spectrogram->setScale(static_cast<double>(m_pipeline->hopSamples()) / static_cast<double>(rate),
                                displayedMaxHz(rate, AppSettings::maxFrequencyHz()));
    }
    setRunning(true);
}

// ================================================================================

void MainWindow::stopCapture() {
    m_pipeline->stop();
    setRunning(false);
}

// ================================================================================

void MainWindow::openSettings() {
    const AudioPermission::Status current = AudioPermission::status();
    if (current == AudioPermission::Status::Undetermined) {
        AudioPermission::request([this](AudioPermission::Status result) {
            if (result != AudioPermission::Status::Granted) {
                showPermissionDialog();
            }
            SettingsDialog dialog(this);
            dialog.exec();
            refreshScale();
        });
        return;
    }
    if (current != AudioPermission::Status::Granted) {
        showPermissionDialog();
    }
    SettingsDialog dialog(this);
    dialog.exec();
    refreshScale();
}

// ================================================================================

void MainWindow::refreshScale() {
    if (m_spectrogram == nullptr || m_pipeline->isRunning() == true) {
        return;
    }
    const int rate = AppSettings::sampleRate();
    const int hop = waterfallHop(AppSettings::fftSize(), AppSettings::overlapPercent());
    if (rate < 2 || hop < 1) {
        return;
    }
    m_spectrogram->setScale(static_cast<double>(hop) / static_cast<double>(rate),
                            displayedMaxHz(rate, AppSettings::maxFrequencyHz()));
}

// ================================================================================

void MainWindow::ensureMicrophonePermission() {
    const AudioPermission::Status current = AudioPermission::status();
    if (current == AudioPermission::Status::Granted) {
        return;
    }
    if (current == AudioPermission::Status::Undetermined) {
        AudioPermission::request([this](AudioPermission::Status result) {
            if (result != AudioPermission::Status::Granted) {
                showPermissionDialog();
            }
        });
        return;
    }
    showPermissionDialog();
}

// ================================================================================

void MainWindow::showPermissionDialog() {
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Microphone access"));
    box.setText(AudioPermission::instructions());
    box.addButton(tr("Close"), QMessageBox::RejectRole);
    QPushButton* openSettings = nullptr;
    if (AudioPermission::canOpenSystemSettings()) {
        openSettings = box.addButton(tr("Open settings"), QMessageBox::ActionRole);
    }
    execMessage(box);
    if (openSettings != nullptr && box.clickedButton() == openSettings) {
        AudioPermission::openSystemSettings();
    }
}

// ================================================================================

void MainWindow::showAudioError(const QString& text) {
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Audio"));
    box.setText(text);
    box.addButton(tr("Close"), QMessageBox::RejectRole);
    execMessage(box);
}
