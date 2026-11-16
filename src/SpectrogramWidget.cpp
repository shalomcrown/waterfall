#include "SpectrogramWidget.h"

#include "WaterfallView.h"

#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QRgb>

#include <algorithm>
#include <cmath>

namespace {
QString formatHz(double hz) {
#if defined(Q_OS_ANDROID)
    if (hz >= 1000.0) {
        return QString::number(static_cast<int>(std::lround(hz / 1000.0))) + QStringLiteral("k");
    }
    return QString::number(static_cast<int>(std::lround(hz)));
#else
    if (hz >= 1000.0) {
        const double khz = hz / 1000.0;
        if (std::abs(khz - std::round(khz)) < 0.05) {
            return QString::number(static_cast<int>(std::lround(khz))) + QStringLiteral(" kHz");
        }
        return QString::number(khz, 'f', 1) + QStringLiteral(" kHz");
    }
    return QString::number(static_cast<int>(std::lround(hz))) + QStringLiteral(" Hz");
#endif
}

// ================================================================================

double tickStep(double span) {
    if (span <= 0.0) {
        return 1.0;
    }
    const double raw = span / 4.0;
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    const double normalized = raw / magnitude;
    double nice = 1.0;
    if (normalized > 5.0) {
        nice = 10.0;
    } else if (normalized > 2.0) {
        nice = 5.0;
    } else if (normalized > 1.0) {
        nice = 2.0;
    }
    return nice * magnitude;
}

// ================================================================================

class FrequencyAxis : public QWidget {
public:
    explicit FrequencyAxis(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("frequencyAxis"));
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        fitWidth();
    }

    void setMaximumHz(double hz) {
        m_maximumHz = hz;
        fitWidth();
        update();
    }

    void fitWidth() {
        QFont font;
#if defined(Q_OS_ANDROID)
        font.setPixelSize(11);
        const int tick = 8;
        const QString sample = QStringLiteral("24k");
#else
        font.setPixelSize(12);
        const int tick = 14;
        const QString sample = QStringLiteral("24 kHz");
#endif
        const int text = QFontMetrics(font).horizontalAdvance(sample);
        setFixedWidth(std::max(text + tick, 28));
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), WaterfallView::background());
        if (m_maximumHz <= 0.0 || height() < 8) {
            return;
        }
        painter.setPen(QColor(186, 196, 206));
        QFont font = painter.font();
#if defined(Q_OS_ANDROID)
        font.setPixelSize(11);
#else
        font.setPixelSize(12);
#endif
        painter.setFont(font);
        const double step = tickStep(m_maximumHz);
        for (double hz = 0.0; hz <= m_maximumHz + step * 0.01; hz += step) {
            const double shown = std::min(hz, m_maximumHz);
            const double fromBottom = shown / m_maximumHz;
            const int y = height() - 1 - static_cast<int>(std::lround(fromBottom * (height() - 1)));
            const int tick = width() > 40 ? 8 : 5;
            painter.drawLine(width() - tick, y, width() - 1, y);
            const QString label = formatHz(shown);
            const QRect bounds(0, y - 8, width() - tick - 1, 16);
            painter.drawText(bounds, Qt::AlignRight | Qt::AlignVCenter, label);
            if (shown >= m_maximumHz) {
                break;
            }
        }
    }

private:
    double m_maximumHz = 0;
};

// ================================================================================

class TimeAxis : public QWidget {
public:
    explicit TimeAxis(WaterfallView* view, QWidget* parent = nullptr) : QWidget(parent), m_view(view) {
        setObjectName(QStringLiteral("timeAxis"));
        setFixedHeight(28);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setSecondsPerColumn(double seconds) {
        m_secondsPerColumn = seconds;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), WaterfallView::background());
        if (m_view == nullptr || m_secondsPerColumn <= 0.0 || width() < 8) {
            return;
        }
        const QImage& image = m_view->image();
        if (image.isNull() == true || image.width() < 1) {
            return;
        }
        const int filled = m_view->columnsFilled();
        if (filled < 1) {
            return;
        }
        painter.setPen(QColor(186, 196, 206));
        QFont font = painter.font();
        font.setPixelSize(12);
        painter.setFont(font);

        const int visibleColumns = std::min(filled, image.width());
        const double visibleSeconds = visibleColumns * m_secondsPerColumn;
        const double step = tickStep(std::max(visibleSeconds, m_secondsPerColumn));
        const double widgetPerImage = static_cast<double>(width()) / static_cast<double>(image.width());
        const int newest = image.width() - 1;
        const int oldest = image.width() - visibleColumns;
        for (double age = 0.0; age <= visibleSeconds + step * 0.01; age += step) {
            // The right edge is the newest sample. Older samples sit to its left.
            const int imageX = newest - static_cast<int>(std::lround(age / m_secondsPerColumn));
            if (imageX < oldest) {
                break;
            }
            const int x = static_cast<int>(std::lround((imageX + 0.5) * widgetPerImage));
            painter.drawLine(x, 0, x, 6);
            const QString label = QString::number(age, 'f', age < 10.0 ? 1 : 0) + QStringLiteral(" s");
            QRect bounds(x - 28, 8, 56, 16);
            if (bounds.left() < 0) {
                bounds.moveLeft(0);
            }
            if (bounds.right() > width()) {
                bounds.moveRight(width() - 1);
            }
            painter.drawText(bounds, Qt::AlignHCenter | Qt::AlignVCenter, label);
        }
    }

private:
    WaterfallView* m_view = nullptr;
    double m_secondsPerColumn = 0;
};
}  // namespace

// ================================================================================

SpectrogramWidget::SpectrogramWidget(QWidget* parent) : QWidget(parent) {
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(4);
    m_waterfall = new WaterfallView(this);
    auto* frequency = new FrequencyAxis(this);
    auto* time = new TimeAxis(m_waterfall, this);
    m_frequency = frequency;
    m_time = time;
    grid->addWidget(frequency, 0, 0);
    grid->addWidget(m_waterfall, 0, 1);
    grid->addWidget(time, 1, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(0, 1);
}

// ================================================================================

void SpectrogramWidget::appendColumn(std::span<const QRgb> bins) {
    m_waterfall->appendColumn(bins);
    if (m_time != nullptr) {
        m_time->update();
    }
}

// ================================================================================

void SpectrogramWidget::setScale(double secondsPerColumn, double maximumHz) {
    m_secondsPerColumn = secondsPerColumn;
    m_maximumHz = maximumHz;
    if (auto* frequency = dynamic_cast<FrequencyAxis*>(m_frequency)) {
        frequency->setMaximumHz(maximumHz);
    }
    if (auto* time = dynamic_cast<TimeAxis*>(m_time)) {
        time->setSecondsPerColumn(secondsPerColumn);
    }
}
