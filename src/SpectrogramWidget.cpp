#include "SpectrogramWidget.h"

#include "FftTiming.h"
#include "WaterfallView.h"

#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QRgb>

#include <algorithm>
#include <cmath>
#include <vector>

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

int axisY(double value, double maximum, int height) {
    const double across = value / maximum;
    return height - 1 - static_cast<int>(std::lround(across * static_cast<double>(height - 1)));
}

// ================================================================================

int axisX(double value, double maximum, int width) {
    const double across = value / maximum;
    return width - 1 - static_cast<int>(std::lround(across * static_cast<double>(width - 1)));
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
        setRange(hz, false, 5.0);
    }

    void setRange(double hz, bool logarithmic, double floorHz) {
        m_maximumHz = hz;
        m_logarithmic = logarithmic;
        m_floorHz = floorHz;
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
        if (m_logarithmic == true) {
            paintLogarithmic(painter);
            return;
        }
        const double step = tickStep(m_maximumHz);
        const int major = width() > 40 ? 8 : 5;
        const int minor = std::max(3, major / 2);
        for (double hz = 0.0; hz <= m_maximumHz + step * 0.01; hz += step) {
            const double shown = std::min(hz, m_maximumHz);
            const int y = axisY(shown, m_maximumHz, height());
            painter.drawLine(width() - major, y, width() - 1, y);
            const QString label = formatHz(shown);
            const QRect bounds(0, y - 8, width() - major - 1, 16);
            painter.drawText(bounds, Qt::AlignRight | Qt::AlignVCenter, label);
            if (shown >= m_maximumHz) {
                break;
            }
            for (int division = 1; division < 5; ++division) {
                const double mark = hz + step * static_cast<double>(division) / 5.0;
                if (mark >= m_maximumHz) {
                    break;
                }
                const int markY = axisY(mark, m_maximumHz, height());
                painter.drawLine(width() - minor, markY, width() - 1, markY);
            }
        }
    }

private:
    void paintLogarithmic(QPainter& painter) const {
        const double floor = clampedLogFloorHz(m_maximumHz, m_floorHz);
        if (m_maximumHz <= floor) {
            return;
        }
        struct Mark {
            double hz = 0;
            bool labeled = false;
        };
        std::vector<Mark> marks;
        const double firstDecade = std::pow(10.0, std::floor(std::log10(floor)));
        for (double decade = firstDecade; decade <= m_maximumHz * 1.001; decade *= 10.0) {
            for (int digit = 1; digit <= 9; ++digit) {
                const double hz = static_cast<double>(digit) * decade;
                if (hz < floor * 0.999 || hz > m_maximumHz * 1.001) {
                    continue;
                }
                marks.push_back(Mark{hz, digit == 1 || digit == 2 || digit == 5});
            }
        }
        bool topLabeled = false;
        for (Mark& mark : marks) {
            if (std::abs(mark.hz - m_maximumHz) / m_maximumHz < 0.02) {
                mark.labeled = true;
                topLabeled = true;
            }
        }
        if (topLabeled == false) {
            marks.push_back(Mark{m_maximumHz, true});
        }
        const int major = width() > 40 ? 8 : 5;
        const int minor = std::max(3, major / 2);
        int previousTop = height() + 20;
        for (const Mark& mark : marks) {
            const double fromBottom = frequencyFromBottom(mark.hz, m_maximumHz, true, floor);
            const int y = height() - 1 - static_cast<int>(std::lround(fromBottom * static_cast<double>(height() - 1)));
            const int length = mark.labeled ? major : minor;
            painter.drawLine(width() - length, y, width() - 1, y);
            if (mark.labeled == false) {
                continue;
            }
            const QString label = formatHz(mark.hz);
            QRect bounds(0, y - 8, width() - major - 1, 16);
            if (bounds.top() < 0) {
                bounds.moveTop(0);
            }
            if (bounds.bottom() > height() - 1) {
                bounds.moveBottom(height() - 1);
            }
            if (bounds.bottom() > previousTop - 2) {
                continue;
            }
            painter.drawText(bounds, Qt::AlignRight | Qt::AlignVCenter, label);
            previousTop = bounds.top();
        }
    }

    double m_maximumHz = 0;
    bool m_logarithmic = false;
    double m_floorHz = 5;
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
        painter.setPen(QColor(186, 196, 206));
        QFont font = painter.font();
        font.setPixelSize(12);
        painter.setFont(font);
        const QFontMetrics metrics(font);

        // Labels stay put for the whole width. Spacing follows that full span,
        // so the first columns do not pile every tick on the right edge.
        const double maximumSeconds = static_cast<double>(image.width()) * m_secondsPerColumn;
        const double step = tickStep(maximumSeconds);
        int previousLeft = width();
        for (double age = 0.0; age <= maximumSeconds + step * 0.01; age += step) {
            const int x = axisX(age, maximumSeconds, width());
            painter.drawLine(x, 0, x, 6);
            for (int division = 1; division < 5; ++division) {
                const double mark = age + step * static_cast<double>(division) / 5.0;
                if (mark >= maximumSeconds) {
                    break;
                }
                const int markX = axisX(mark, maximumSeconds, width());
                painter.drawLine(markX, 0, markX, 3);
            }
            const QString label = QString::number(age, 'f', age < 10.0 ? 1 : 0) + QStringLiteral(" s");
            const int textWidth = metrics.horizontalAdvance(label);
            QRect bounds(x - textWidth / 2, 8, textWidth, 16);
            if (bounds.left() < 0) {
                bounds.moveLeft(0);
            }
            if (bounds.right() > width() - 1) {
                bounds.moveRight(width() - 1);
            }
            if (bounds.right() >= previousLeft) {
                continue;
            }
            painter.drawText(bounds, Qt::AlignHCenter | Qt::AlignVCenter, label);
            previousLeft = bounds.left() - 6;
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

void SpectrogramWidget::setScale(double secondsPerColumn, double maximumHz, bool logarithmic, double floorHz) {
    m_secondsPerColumn = secondsPerColumn;
    m_maximumHz = maximumHz;
    if (auto* frequency = dynamic_cast<FrequencyAxis*>(m_frequency)) {
        frequency->setRange(maximumHz, logarithmic, floorHz);
    }
    if (auto* time = dynamic_cast<TimeAxis*>(m_time)) {
        time->setSecondsPerColumn(secondsPerColumn);
    }
}
