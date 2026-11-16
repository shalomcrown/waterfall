#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QPushButton;
class QShowEvent;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    QComboBox* inputCombo() const { return m_input; }
    QComboBox* outputCombo() const { return m_output; }
    QComboBox* sampleRateCombo() const { return m_sampleRate; }
    QComboBox* fftCombo() const { return m_fft; }
    QComboBox* overlapCombo() const { return m_overlap; }
    QComboBox* maxFrequencyCombo() const { return m_maxFrequency; }
    QCheckBox* keepScreenCheck() const { return m_keepScreen; }
    QComboBox* paletteCombo() const { return m_palette; }
    QPushButton* closeButton() const { return m_close; }

protected:
    void showEvent(QShowEvent* event) override;

private:
    void reloadDevices();
    void reloadSampleRates();
    void selectSaved(QComboBox* combo, const QVariant& saved, const QVariant& fallback);

    QComboBox* m_input = nullptr;
    QComboBox* m_output = nullptr;
    QComboBox* m_sampleRate = nullptr;
    QComboBox* m_fft = nullptr;
    QComboBox* m_overlap = nullptr;
    QComboBox* m_maxFrequency = nullptr;
    QCheckBox* m_keepScreen = nullptr;
    QComboBox* m_palette = nullptr;
    QPushButton* m_close = nullptr;
    bool m_fullScreenApplied = false;
};
