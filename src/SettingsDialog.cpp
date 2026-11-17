#include "SettingsDialog.h"

#include "AppSettings.h"
#include "RecordingSave.h"
#include "ScreenAwake.h"

#include <QAudioDevice>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaDevices>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

namespace {
QComboBox* makeCombo(QWidget* parent, const char* name) {
    auto* combo = new QComboBox(parent);
    combo->setObjectName(QString::fromUtf8(name));
    combo->setMinimumHeight(44);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return combo;
}

// ================================================================================

void fillDevices(QComboBox* combo, const QList<QAudioDevice>& devices, const QString& savedId) {
    QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(QObject::tr("System default"), QString());
    int selected = 0;
    for (const QAudioDevice& device : devices) {
        const QString id = QString::fromUtf8(device.id());
        combo->addItem(device.description(), id);
        if (savedId.isEmpty() == false && id == savedId) {
            selected = combo->count() - 1;
        }
    }
    combo->setCurrentIndex(selected);
}

// ================================================================================

QAudioDevice deviceById(const QList<QAudioDevice>& devices, const QString& id, const QAudioDevice& fallback) {
    if (id.isEmpty()) {
        return fallback;
    }
    const QByteArray key = id.toUtf8();
    for (const QAudioDevice& device : devices) {
        if (device.id() == key) {
            return device;
        }
    }
    return fallback;
}
}  // namespace

// ================================================================================

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Settings"));
    setModal(true);
#if defined(Q_OS_ANDROID)
    // Android draws QDialog without a usable frame. A full-screen page with
    // its own back control is the exit path. Qt::Window is required so the
    // dialog is a top-level page rather than a floating panel.
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
#else
    setMinimumWidth(420);
#endif

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

#if defined(Q_OS_ANDROID)
    auto* header = new QHBoxLayout();
    header->setSpacing(12);
    m_close = new QPushButton(style()->standardIcon(QStyle::SP_ArrowBack), tr("Back"), this);
    m_close->setToolTip(tr("Back"));
    m_close->setAccessibleName(tr("Back"));
    auto* title = new QLabel(tr("Settings"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    title->setFont(titleFont);
    header->addWidget(m_close);
    header->addWidget(title, 1);
    layout->addLayout(header);
    auto* headerRule = new QFrame(this);
    headerRule->setFrameShape(QFrame::HLine);
    headerRule->setFrameShadow(QFrame::Sunken);
    layout->addWidget(headerRule);
#else
    auto* buttons = new QDialogButtonBox(this);
    m_close = buttons->addButton(QDialogButtonBox::Close);
    m_close->setText(tr("Close"));
    m_close->setToolTip(tr("Close"));
    m_close->setAccessibleName(tr("Close"));
    m_close->setAutoDefault(false);
    m_close->setDefault(false);
#endif
    m_close->setObjectName(QStringLiteral("closeSettings"));
    m_close->setMinimumHeight(44);
    connect(m_close, &QPushButton::clicked, this, &QDialog::reject);

    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setSpacing(12);

    m_input = makeCombo(this, "inputDevice");
    m_output = makeCombo(this, "outputDevice");
    m_sampleRate = makeCombo(this, "sampleRate");
    m_fft = makeCombo(this, "fftSize");
    m_overlap = makeCombo(this, "overlap");
    m_maxFrequency = makeCombo(this, "maxFrequency");
    m_frequencyScale = makeCombo(this, "frequencyScale");
    m_logFloor = makeCombo(this, "logFloor");
    m_recording = makeCombo(this, "recordingSeconds");
    m_format = makeCombo(this, "recordingFormat");
    m_keepScreen = new QCheckBox(tr("Keep screen on"), this);
    m_keepScreen->setObjectName(QStringLiteral("keepScreenOn"));
    m_keepScreen->setMinimumHeight(44);
    m_palette = makeCombo(this, "palette");

    form->addRow(tr("Input"), m_input);
    form->addRow(tr("Output"), m_output);
    form->addRow(tr("Sample rate"), m_sampleRate);
    form->addRow(tr("FFT size"), m_fft);
    form->addRow(tr("Overlap"), m_overlap);
    form->addRow(tr("Frequency range"), m_maxFrequency);
    form->addRow(tr("Frequency scale"), m_frequencyScale);
    form->addRow(tr("Log floor"), m_logFloor);
    form->addRow(tr("Save window"), m_recording);
    form->addRow(tr("Save format"), m_format);
    form->addRow(QString(), m_keepScreen);
    form->addRow(tr("Heatmap"), m_palette);
    layout->addLayout(form);
    layout->addStretch(1);

#if !defined(Q_OS_ANDROID)
    auto* rule = new QFrame(this);
    rule->setFrameShape(QFrame::HLine);
    rule->setFrameShadow(QFrame::Sunken);
    layout->addWidget(rule);
    layout->addWidget(buttons);
#endif

    {
        QSignalBlocker blocker(m_fft);
        for (int size : AppSettings::fftSizes()) {
            m_fft->addItem(QString::number(size), size);
        }
        selectSaved(m_fft, AppSettings::fftSize(), 2048);
    }
    {
        QSignalBlocker blocker(m_overlap);
        for (int percent : AppSettings::overlapPercents()) {
            const QString label = percent == 87 ? tr("87.5%") : tr("%1%").arg(percent);
            m_overlap->addItem(label, percent);
        }
        selectSaved(m_overlap, AppSettings::overlapPercent(), 50);
    }
    {
        QSignalBlocker blocker(m_maxFrequency);
        for (int hz : AppSettings::maxFrequenciesHz()) {
            const QString label = hz == 0 ? tr("Full range") : tr("%1 kHz").arg(hz / 1000);
            m_maxFrequency->addItem(label, hz);
        }
        selectSaved(m_maxFrequency, AppSettings::maxFrequencyHz(), 8000);
    }
    {
        QSignalBlocker blocker(m_frequencyScale);
        m_frequencyScale->addItem(tr("Linear"), QStringLiteral("linear"));
        m_frequencyScale->addItem(tr("Logarithmic"), QStringLiteral("log"));
        selectSaved(m_frequencyScale,
                    AppSettings::logarithmicFrequency() ? QStringLiteral("log") : QStringLiteral("linear"),
                    QStringLiteral("linear"));
    }
    {
        QSignalBlocker blocker(m_logFloor);
        for (int hz : AppSettings::logFloorChoices()) {
            m_logFloor->addItem(tr("%1 Hz").arg(hz), hz);
        }
        selectSaved(m_logFloor, AppSettings::logFloorHz(), 5);
        m_logFloor->setEnabled(m_frequencyScale->currentData().toString() == QStringLiteral("log"));
    }
    {
        QSignalBlocker blocker(m_recording);
        for (int seconds : AppSettings::recordingSecondsChoices()) {
            const QString label = seconds == 60 ? tr("1 minute") : tr("%1 minutes").arg(seconds / 60);
            m_recording->addItem(label, seconds);
        }
        selectSaved(m_recording, AppSettings::recordingSeconds(), 300);
    }
    {
        QSignalBlocker blocker(m_format);
        for (const RecordingFormat& format : RecordingSave::formats()) {
            m_format->addItem(format.label, format.id);
        }
        selectSaved(m_format, AppSettings::recordingFormat(), QStringLiteral("wav"));
    }
    {
        QSignalBlocker blocker(m_keepScreen);
        m_keepScreen->setChecked(AppSettings::keepScreenOn());
    }
    {
        QSignalBlocker blocker(m_palette);
        for (const QString& name : AppSettings::paletteNames()) {
            m_palette->addItem(name, name);
        }
        selectSaved(m_palette, AppSettings::palette(), QStringLiteral("Viridis"));
    }
    reloadDevices();

    connect(m_input, &QComboBox::currentIndexChanged, this, [this] {
        AppSettings::setInputDeviceId(m_input->currentData().toString());
        reloadSampleRates();
    });
    connect(m_output, &QComboBox::currentIndexChanged, this, [this] {
        AppSettings::setOutputDeviceId(m_output->currentData().toString());
    });
    connect(m_sampleRate, &QComboBox::currentIndexChanged, this, [this] {
        AppSettings::setSampleRate(m_sampleRate->currentData().toInt());
    });
    connect(m_fft, &QComboBox::currentIndexChanged, this, [this] {
        AppSettings::setFftSize(m_fft->currentData().toInt());
    });
    connect(m_overlap, &QComboBox::currentIndexChanged, this, [this] {
        AppSettings::setOverlapPercent(m_overlap->currentData().toInt());
    });
    connect(m_maxFrequency, &QComboBox::currentIndexChanged, this, [this] {
        AppSettings::setMaxFrequencyHz(m_maxFrequency->currentData().toInt());
    });
    connect(m_frequencyScale, &QComboBox::currentIndexChanged, this, [this] {
        const bool logarithmic = m_frequencyScale->currentData().toString() == QStringLiteral("log");
        AppSettings::setLogarithmicFrequency(logarithmic);
        m_logFloor->setEnabled(logarithmic);
    });
    connect(m_logFloor, &QComboBox::currentIndexChanged, this, [this] {
        AppSettings::setLogFloorHz(m_logFloor->currentData().toInt());
    });
    connect(m_recording, &QComboBox::currentIndexChanged, this, [this] {
        AppSettings::setRecordingSeconds(m_recording->currentData().toInt());
    });
    connect(m_format, &QComboBox::currentIndexChanged, this, [this] {
        AppSettings::setRecordingFormat(m_format->currentData().toString());
    });
    connect(m_keepScreen, &QCheckBox::toggled, this, [](bool checked) {
        AppSettings::setKeepScreenOn(checked);
        ScreenAwake::setKeepAwake(checked);
    });
    connect(m_palette, &QComboBox::currentIndexChanged, this, [this] {
        AppSettings::setPalette(m_palette->currentData().toString());
    });
}

// ================================================================================

void SettingsDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
#if defined(Q_OS_ANDROID)
    if (m_fullScreenApplied == true) {
        return;
    }
    m_fullScreenApplied = true;
    if (QScreen* screen = this->screen()) {
        setGeometry(screen->geometry());
        connect(screen, &QScreen::geometryChanged, this, [this](const QRect& geometry) {
            setGeometry(geometry);
            setWindowState(windowState() | Qt::WindowFullScreen);
        });
    }
    setWindowState(windowState() | Qt::WindowFullScreen);
#else
    Q_UNUSED(m_fullScreenApplied);
#endif
}

// ================================================================================

void SettingsDialog::reloadDevices() {
    fillDevices(m_input, QMediaDevices::audioInputs(), AppSettings::inputDeviceId());
    fillDevices(m_output, QMediaDevices::audioOutputs(), AppSettings::outputDeviceId());
    reloadSampleRates();
}

// ================================================================================

void SettingsDialog::reloadSampleRates() {
    const QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
    const QAudioDevice device =
            deviceById(inputs, m_input->currentData().toString(), QMediaDevices::defaultAudioInput());
    int minimum = 8000;
    int maximum = 192000;
    if (device.isNull() == false) {
        if (device.minimumSampleRate() > 0) {
            minimum = device.minimumSampleRate();
        }
        if (device.maximumSampleRate() > 0) {
            maximum = device.maximumSampleRate();
        }
    }

    QSignalBlocker blocker(m_sampleRate);
    m_sampleRate->clear();
    for (int rate : AppSettings::sampleRates()) {
        if (rate < minimum || rate > maximum) {
            continue;
        }
        m_sampleRate->addItem(tr("%1 Hz").arg(rate), rate);
    }
    if (m_sampleRate->count() == 0) {
        m_sampleRate->addItem(tr("%1 Hz").arg(48000), 48000);
    }
    selectSaved(m_sampleRate, AppSettings::sampleRate(), 48000);
}

// ================================================================================

void SettingsDialog::selectSaved(QComboBox* combo, const QVariant& saved, const QVariant& fallback) {
    int index = combo->findData(saved);
    if (index < 0) {
        index = combo->findData(fallback);
    }
    combo->setCurrentIndex(index < 0 ? 0 : index);
}
