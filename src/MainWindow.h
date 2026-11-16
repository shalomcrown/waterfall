#pragma once

#include <QMainWindow>

class QPushButton;
class AudioPipeline;
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
    WaterfallView* waterfall() const { return m_waterfall; }

private:
    void setRunning(bool running);
    void startCapture();
    void stopCapture();
    void openSettings();
    void ensureMicrophonePermission();
    void showPermissionDialog();
    void showAudioError(const QString& text);

    WaterfallView* m_waterfall = nullptr;
    AudioPipeline* m_pipeline = nullptr;
    QPushButton* m_start = nullptr;
    QPushButton* m_stop = nullptr;
    QPushButton* m_save = nullptr;
    QPushButton* m_load = nullptr;
    QPushButton* m_configure = nullptr;
};
