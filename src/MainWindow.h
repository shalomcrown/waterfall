#pragma once

#include <QMainWindow>

class QShowEvent;

class QPushButton;
class AudioPipeline;
class SpectrogramWidget;
class WaterfallView;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    QPushButton* startButton() const { return m_start; }
    QPushButton* stopButton() const { return m_stop; }
    QPushButton* saveButton() const { return m_save; }
    QPushButton* loadButton() const { return m_load; }
    QPushButton* configureButton() const { return m_configure; }
    QPushButton* aboutButton() const { return m_about; }
    WaterfallView* waterfall() const { return m_waterfall; }
    bool loadPath(const QString& path, QString& error);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void applyTransport();
    void startCapture();
    void stopCapture();
    void togglePlayback();
    void onPrimary();
    void onSecondary();
    void loadRecording();
    void saveRecording();
    void openSettings();
    void openAbout();
    void ensureMicrophonePermission();
    void showPermissionDialog();
    void showAudioError(const QString& text);
    void refreshScale();
    void beginView(bool fromFile);
    void applyKeepAwake();

    bool m_saving = false;
    bool m_frequencyScaleReady = false;
    bool m_logarithmic = false;
    int m_logFloorHz = 5;

    SpectrogramWidget* m_spectrogram = nullptr;
    WaterfallView* m_waterfall = nullptr;
    AudioPipeline* m_pipeline = nullptr;
    QPushButton* m_start = nullptr;
    QPushButton* m_stop = nullptr;
    QPushButton* m_save = nullptr;
    QPushButton* m_load = nullptr;
    QPushButton* m_configure = nullptr;
    QPushButton* m_about = nullptr;
};
