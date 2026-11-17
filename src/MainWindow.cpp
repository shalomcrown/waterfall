#include "MainWindow.h"

#include "AboutDialog.h"
#include "AppIcon.h"
#include "AppSettings.h"
#include "AudioClip.h"
#include "AudioPermission.h"
#include "AudioPipeline.h"
#include "FftTiming.h"
#include "RecordingSave.h"
#include "ScreenAwake.h"
#include "SettingsDialog.h"
#include "SpectrogramWidget.h"
#include "WaterfallView.h"

#include <QApplication>
#include <QByteArray>
#include <QGuiApplication>
#include <QShowEvent>
#include <QAudioDevice>
#include <QDir>
#include <QFileDialog>
#include <QIcon>
#include <QStandardPaths>
#include <QHBoxLayout>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <span>

#if defined(Q_OS_ANDROID)
#include "AndroidDocument.h"
#endif

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

QAudioDevice selectedOutputDevice() {
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    const QString saved = AppSettings::outputDeviceId();
    if (saved.isEmpty() == false) {
        for (const QAudioDevice& device : outputs) {
            if (QString::fromUtf8(device.id()) == saved) {
                return device;
            }
        }
    }
    return QMediaDevices::defaultAudioOutput();
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
    m_about = makeButton(central, QStringLiteral("About"),
                         themedIcon(style, "help-about", QStyle::SP_MessageBoxInformation));
    controls->addWidget(m_start);
    controls->addWidget(m_stop);
    controls->addWidget(m_save);
    controls->addWidget(m_load);
    controls->addWidget(m_configure);
    controls->addWidget(m_about);
    controls->addStretch(1);
    layout->addLayout(controls);

    setCentralWidget(central);
    resize(800, 480);

    m_pipeline = new AudioPipeline(this);
    connect(m_pipeline, &AudioPipeline::columnReady, this, [this](const QVector<QRgb>& column) {
        if (m_spectrogram == nullptr || column.isEmpty() == true) {
            return;
        }
        m_spectrogram->appendColumn(std::span<const QRgb>(column.constData(), static_cast<size_t>(column.size())));
    });
    refreshScale();
    applyTransport();
    applyKeepAwake();
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            applyKeepAwake();
        }
    });
    QTimer::singleShot(800, this, &MainWindow::applyKeepAwake);
    QTimer::singleShot(2000, this, &MainWindow::applyKeepAwake);

    connect(m_pipeline, &AudioPipeline::playbackFinished, this, &MainWindow::applyTransport);
    connect(m_start, &QPushButton::clicked, this, &MainWindow::onPrimary);
    connect(m_stop, &QPushButton::clicked, this, &MainWindow::onSecondary);
    connect(m_save, &QPushButton::clicked, this, &MainWindow::saveRecording);
    connect(m_load, &QPushButton::clicked, this, &MainWindow::loadRecording);
    connect(m_configure, &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(m_about, &QPushButton::clicked, this, &MainWindow::openAbout);
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

void MainWindow::applyTransport() {
    QStyle* style = this->style();
    const auto setButton = [style](QPushButton* button, const QString& text, const char* theme,
                                   QStyle::StandardPixmap pixmap, bool enabled) {
        button->setText(text);
        button->setIcon(themedIcon(style, theme, pixmap));
        button->setToolTip(text);
        button->setAccessibleName(text);
        button->setEnabled(enabled);
    };
    if (m_pipeline->clipLoaded() == true) {
        if (m_pipeline->isPlaying() == true) {
            setButton(m_start, tr("Pause"), "media-playback-pause", QStyle::SP_MediaPause, true);
        } else {
            setButton(m_start, tr("Play"), "media-playback-start", QStyle::SP_MediaPlay, true);
        }
        setButton(m_stop, tr("Live"), "audio-input-microphone", QStyle::SP_MediaVolume, true);
        return;
    }
    const bool running = m_pipeline->isRunning();
    setButton(m_start, tr("Start"), "media-playback-start", QStyle::SP_MediaPlay, running == false);
    setButton(m_stop, tr("Stop"), "media-playback-stop", QStyle::SP_MediaStop, running);
}

// ================================================================================

void MainWindow::onPrimary() {
    if (m_pipeline->clipLoaded() == true) {
        togglePlayback();
        return;
    }
    startCapture();
}

// ================================================================================

void MainWindow::onSecondary() {
    if (m_pipeline->clipLoaded() == true) {
        startCapture();
        return;
    }
    stopCapture();
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
    m_pipeline->setRecordingSeconds(AppSettings::recordingSeconds());
    m_pipeline->setFrequencyDisplay(AppSettings::logarithmicFrequency(), AppSettings::logFloorHz());
    const bool started =
            m_pipeline->start(selectedInputDevice(), AppSettings::sampleRate(), AppSettings::fftSize(),
                              AppSettings::overlapPercent(), AppSettings::maxFrequencyHz(), AppSettings::palette(), error);
    if (started == false) {
        applyTransport();
        if (error.isEmpty() == true) {
            error = tr("Audio capture did not start.");
        }
        showAudioError(error);
        return;
    }
    m_pipeline->clearClip();
    const int rate = m_pipeline->captureSampleRate();
    if (rate > 1 && m_pipeline->hopSamples() > 0) {
        m_spectrogram->setScale(static_cast<double>(m_pipeline->hopSamples()) / static_cast<double>(rate),
                                displayedMaxHz(rate, AppSettings::maxFrequencyHz()), AppSettings::logarithmicFrequency(),
                                AppSettings::logFloorHz());
    }
    m_logarithmic = AppSettings::logarithmicFrequency();
    m_logFloorHz = AppSettings::logFloorHz();
    m_frequencyScaleReady = true;
    applyTransport();
}

// ================================================================================

void MainWindow::stopCapture() {
    m_pipeline->stop();
    applyTransport();
}

// ================================================================================

void MainWindow::togglePlayback() {
    if (m_pipeline->isPlaying() == true) {
        m_pipeline->pauseClip();
        applyTransport();
        return;
    }
    const bool fromStart = m_pipeline->clipPosition() == 0 || m_pipeline->clipPosition() >= m_pipeline->clipLength();
    QString error;
    m_pipeline->setRecordingSeconds(AppSettings::recordingSeconds());
    m_pipeline->setFrequencyDisplay(AppSettings::logarithmicFrequency(), AppSettings::logFloorHz());
    const bool started =
            m_pipeline->playClip(selectedOutputDevice(), AppSettings::fftSize(), AppSettings::overlapPercent(),
                                 AppSettings::maxFrequencyHz(), AppSettings::palette(), error);
    if (started == false) {
        applyTransport();
        if (error.isEmpty() == true) {
            error = tr("Playback did not start.");
        }
        showAudioError(error);
        return;
    }
    if (fromStart == true && m_waterfall != nullptr) {
        m_waterfall->clear();
    }
    const int rate = m_pipeline->clipSampleRate();
    if (rate > 1 && m_pipeline->hopSamples() > 0 && m_spectrogram != nullptr) {
        m_spectrogram->setScale(static_cast<double>(m_pipeline->hopSamples()) / static_cast<double>(rate),
                                displayedMaxHz(rate, AppSettings::maxFrequencyHz()), AppSettings::logarithmicFrequency(),
                                AppSettings::logFloorHz());
    }
    m_logarithmic = AppSettings::logarithmicFrequency();
    m_logFloorHz = AppSettings::logFloorHz();
    m_frequencyScaleReady = true;
    applyTransport();
}

// ================================================================================

bool MainWindow::loadPath(const QString& path, QString& error) {
    AudioClip clip;
    if (AudioClip::load(path, clip, error) == false) {
        return false;
    }
    if (m_pipeline->loadClip(clip.samples, clip.sampleRate, error) == false) {
        return false;
    }
    applyTransport();
    return true;
}

// ================================================================================

void MainWindow::loadRecording() {
    QString error;
#if defined(Q_OS_ANDROID)
    QString uri;
    if (androidOpenDocument(uri, error) == false) {
        if (error.isEmpty() == false) {
            showAudioError(error);
        }
        return;
    }
    QByteArray bytes;
    if (androidReadDocument(uri, bytes, error) == false) {
        showAudioError(error);
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    AudioClip clip;
    const bool decoded = AudioClip::decode(bytes, clip, error);
    QApplication::restoreOverrideCursor();
    if (decoded == false) {
        showAudioError(error);
        return;
    }
    if (m_pipeline->loadClip(clip.samples, clip.sampleRate, error) == false) {
        showAudioError(error);
        return;
    }
    applyTransport();
#else
    QString directory = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (directory.isEmpty() == true) {
        directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    QFileDialog dialog(this, tr("Open recording"));
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters({tr("WAV and MP3 (*.wav *.mp3)"), tr("WAV (*.wav)"), tr("MP3 (*.mp3)")});
    if (directory.isEmpty() == false) {
        dialog.setDirectory(directory);
    }
    if (QApplication::platformName() == QStringLiteral("offscreen")) {
        QTimer::singleShot(0, &dialog, &QDialog::reject);
    }
    if (dialog.exec() == QDialog::Rejected) {
        return;
    }
    const QStringList files = dialog.selectedFiles();
    if (files.isEmpty() == true) {
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool loaded = loadPath(files.first(), error);
    QApplication::restoreOverrideCursor();
    if (loaded == false) {
        showAudioError(error);
    }
#endif
}

// ================================================================================

void MainWindow::saveRecording() {
    if (m_saving == true) {
        return;
    }
    const auto samples = m_pipeline->copyRecording();
    const int rate = m_pipeline->recordingSampleRate();
    if (samples.empty() == true || rate < 1) {
        showAudioError(tr("There is no audio to save. Start capture first."));
        return;
    }
    m_saving = true;
    m_save->setEnabled(false);
    QString error;
    const bool saved = RecordingSave::promptAndWrite(this, rate, samples, error);
    m_save->setEnabled(true);
    m_saving = false;
    if (saved == false && error.isEmpty() == false) {
        showAudioError(error);
    }
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

void MainWindow::openAbout() {
    AboutDialog dialog(this);
    dialog.exec();
}

// ================================================================================

void MainWindow::refreshScale() {
    if (m_spectrogram == nullptr || m_pipeline == nullptr) {
        return;
    }
    const bool logarithmic = AppSettings::logarithmicFrequency();
    const int floor = AppSettings::logFloorHz();
    m_pipeline->setFrequencyDisplay(logarithmic, floor);
    const bool live = m_pipeline->isRunning() == true || m_pipeline->isPlaying() == true;
    double seconds = m_spectrogram->secondsPerColumn();
    double maximum = m_spectrogram->maximumHz();
    if (live == false) {
        const int rate = m_pipeline->clipLoaded() == true ? m_pipeline->clipSampleRate() : AppSettings::sampleRate();
        const int hop = waterfallHop(AppSettings::fftSize(), AppSettings::overlapPercent());
        if (rate > 1 && hop > 0) {
            seconds = static_cast<double>(hop) / static_cast<double>(rate);
            maximum = displayedMaxHz(rate, AppSettings::maxFrequencyHz());
        }
    }
    m_spectrogram->setScale(seconds, maximum, logarithmic, floor);
    if (m_frequencyScaleReady == true && (logarithmic != m_logarithmic || floor != m_logFloorHz) &&
        m_waterfall != nullptr) {
        m_waterfall->clear();
    }
    m_frequencyScaleReady = true;
    m_logarithmic = logarithmic;
    m_logFloorHz = floor;
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
