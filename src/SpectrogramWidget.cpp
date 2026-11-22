#include "SpectrogramWidget.h"

#include "FftTiming.h"
#include "TimeLabels.h"
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
QString trimZeros(QString text) {
    if (text.contains(QLatin1Char('.')) == true) {
        while (text.endsWith(QLatin1Char('0')) == true) {
            text.chop(1);
        }
        if (text.endsWith(QLatin1Char('.')) == true) {
            text.chop(1);
        }
    }
    return text;
}

// ================================================================================

// step is the distance between labels. It decides how many decimals a kHz label needs.
QString formatHz(double hz, double step) {
    const int decimals = step >= 1000.0 ? 0 : (step >= 100.0 ? 1 : 2);
#if defined(Q_OS_ANDROID)
    if (hz >= 1000.0) {
        return trimZeros(QString::number(hz / 1000.0, 'f', decimals)) + QStringLiteral("k");
    }
    return QString::number(static_cast<int>(std::lround(hz)));
#else
    if (hz >= 1000.0) {
        return trimZeros(QString::number(hz / 1000.0, 'f', decimals)) + QStringLiteral(" kHz");
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

// y for a fraction of the axis, where the window shows low at the bottom and high at the top.
int axisY(double fraction, double low, double high, int height) {
    const double across = (fraction - low) / (high - low);
    return height - 1 - static_cast<int>(std::lround(across * static_cast<double>(height - 1)));
}

// ================================================================================

class FrequencyAxis : public QWidget {
public:
    explicit FrequencyAxis(WaterfallView* view, QWidget* parent = nullptr) : QWidget(parent), m_view(view) {
        setObjectName(QStringLiteral("frequencyAxis"));
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        fitWidth();
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
        const QString sample = QStringLiteral("24.5k");
#else
        font.setPixelSize(12);
        const int tick = 14;
        const QString sample = QStringLiteral("24.5 kHz");
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
        WaterfallFrequencyWindow window;
        if (m_view != nullptr) {
            window = m_view->frequencyWindow();
        }
        const double low = std::clamp(window.low, 0.0, 1.0);
        const double high = std::clamp(window.high, 0.0, 1.0);
        if (high - low < 1e-6) {
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
            paintLogarithmic(painter, low, high);
            return;
        }
        const double lowHz = low * m_maximumHz;
        const double highHz = high * m_maximumHz;
        const double step = tickStep(highHz - lowHz);
        const int major = width() > 40 ? 8 : 5;
        const int minor = std::max(3, major / 2);
        const long long first = static_cast<long long>(std::floor(lowHz / step));
        for (long long index = first; static_cast<double>(index) * step <= highHz + step * 0.001; ++index) {
            const double hz = static_cast<double>(index) * step;
            if (hz >= lowHz - step * 0.001 && hz <= highHz + step * 0.001) {
                const double shown = std::clamp(hz, lowHz, highHz);
                const int y = axisY(shown / m_maximumHz, low, high, height());
                painter.drawLine(width() - major, y, width() - 1, y);
                QRect bounds(0, y - 8, width() - major - 1, 16);
                bounds.moveTop(std::clamp(bounds.top(), 0, std::max(0, height() - 16)));
                painter.drawText(bounds, Qt::AlignRight | Qt::AlignVCenter, formatHz(shown, step));
            }
            for (int division = 1; division < 5; ++division) {
                const double mark = hz + step * static_cast<double>(division) / 5.0;
                if (mark <= lowHz || mark >= highHz) {
                    continue;
                }
                const int markY = axisY(mark / m_maximumHz, low, high, height());
                painter.drawLine(width() - minor, markY, width() - 1, markY);
            }
        }
    }

private:
    void paintLogarithmic(QPainter& painter, double low, double high) const {
        const double floor = clampedLogFloorHz(m_maximumHz, m_floorHz);
        if (m_maximumHz <= floor) {
            return;
        }
        const double lowHz = hzFromBottom(low, m_maximumHz, true, floor);
        const double highHz = hzFromBottom(high, m_maximumHz, true, floor);
        // A narrow window has room for every digit, a wide one only for 1, 2 and 5.
        const bool everyDigit = highHz / std::max(lowHz, 1e-9) < 4.0;
        struct Mark {
            double hz = 0;
            bool labeled = false;
        };
        std::vector<Mark> marks;
        const double firstDecade = std::pow(10.0, std::floor(std::log10(std::max(lowHz, 1e-3))));
        for (double decade = firstDecade; decade <= highHz * 1.001; decade *= 10.0) {
            for (int digit = 1; digit <= 9; ++digit) {
                const double hz = static_cast<double>(digit) * decade;
                if (hz < lowHz * 0.999 || hz > highHz * 1.001 || hz < floor * 0.999 || hz > m_maximumHz * 1.001) {
                    continue;
                }
                marks.push_back(Mark{hz, everyDigit == true || digit == 1 || digit == 2 || digit == 5});
            }
        }
        if (high >= 0.999) {
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
        }
        const int major = width() > 40 ? 8 : 5;
        const int minor = std::max(3, major / 2);
        int previousTop = height() + 20;
        for (const Mark& mark : marks) {
            const double fraction = frequencyFromBottom(mark.hz, m_maximumHz, true, floor);
            const int y = axisY(fraction, low, high, height());
            const int length = mark.labeled ? major : minor;
            painter.drawLine(width() - length, y, width() - 1, y);
            if (mark.labeled == false) {
                continue;
            }
            const QString label = formatHz(mark.hz, 1000.0);
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

    WaterfallView* m_view = nullptr;
    double m_maximumHz = 0;
    bool m_logarithmic = false;
    double m_floorHz = 5;
};

// ================================================================================

// The time scale: the clock for live capture, the position in the file for a file.
class TimeAxis : public QWidget {
public:
    explicit TimeAxis(WaterfallView* view, QWidget* parent = nullptr) : QWidget(parent), m_view(view) {
        setObjectName(QStringLiteral("timeAxis"));
        setFixedHeight(28);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), WaterfallView::background());
        if (m_view == nullptr || width() < 8) {
            return;
        }
        const WaterfallTimeWindow window = m_view->timeWindow();
        const double span = window.rightSeconds - window.leftSeconds;
        if (window.valid == false || span <= 0.0) {
            return;
        }
        painter.setPen(QColor(186, 196, 206));
        QFont font = painter.font();
        font.setPixelSize(12);
        painter.setFont(font);
        const QFontMetrics metrics(font);

        const QString sample = window.clock ? QStringLiteral("00:00:00") : QStringLiteral("00:00");
        const double gap = static_cast<double>(metrics.horizontalAdvance(sample)) + 28.0;
        const double perSecond = static_cast<double>(width() - 1) / span;
        const double step = niceTimeStep(gap / perSecond);
        const long long stepMs = std::max<long long>(1, std::llround(step * 1000.0));
        const long long originMs = window.clock ? window.originLocalMs : 0;
        const long long leftMs = originMs + std::llround(window.leftSeconds * 1000.0);
        const long long rightMs = originMs + std::llround(window.rightSeconds * 1000.0);
        const auto xFor = [&](long long ms) {
            const double seconds = static_cast<double>(ms - originMs) / 1000.0;
            return static_cast<int>(std::lround((seconds - window.leftSeconds) / span * static_cast<double>(width() - 1)));
        };

        struct Tick {
            long long ms = 0;
            int x = 0;
        };
        std::vector<Tick> ticks;
        long long index = leftMs >= 0 ? leftMs / stepMs : -((-leftMs + stepMs - 1) / stepMs);
        for (index -= 1; index * stepMs <= rightMs + stepMs; ++index) {
            const long long ms = index * stepMs;
            if (window.clock == false && ms < 0) {
                continue;
            }
            if (ms >= leftMs - stepMs / 100 && ms <= rightMs + stepMs / 100) {
                ticks.push_back(Tick{ms, xFor(ms)});
            }
            for (int division = 1; division < 5; ++division) {
                const long long mark = ms + stepMs * division / 5;
                if (mark <= leftMs || mark >= rightMs || (window.clock == false && mark < 0)) {
                    continue;
                }
                const int markX = xFor(mark);
                painter.drawLine(markX, 0, markX, 3);
            }
        }
        // The newest label is drawn first, so it is always there when labels are crowded.
        int previousLeft = width();
        for (auto tick = ticks.rbegin(); tick != ticks.rend(); ++tick) {
            painter.drawLine(tick->x, 0, tick->x, 6);
            const QString label = window.clock ? formatClockTime(tick->ms, step)
                                               : formatPosition(static_cast<double>(tick->ms) / 1000.0, step);
            const int textWidth = metrics.horizontalAdvance(label);
            QRect bounds(tick->x - textWidth / 2, 8, textWidth, 16);
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
};
}  // namespace

// ================================================================================

SpectrogramWidget::SpectrogramWidget(QWidget* parent) : QWidget(parent) {
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(4);
    m_waterfall = new WaterfallView(this);
    auto* frequency = new FrequencyAxis(m_waterfall, this);
    auto* time = new TimeAxis(m_waterfall, this);
    m_frequency = frequency;
    m_time = time;
    grid->addWidget(frequency, 0, 0);
    grid->addWidget(m_waterfall, 0, 1);
    grid->addWidget(time, 1, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(0, 1);
    connect(m_waterfall, &WaterfallView::viewChanged, frequency, qOverload<>(&QWidget::update));
    connect(m_waterfall, &WaterfallView::viewChanged, time, qOverload<>(&QWidget::update));
}

// ================================================================================

void SpectrogramWidget::appendColumn(std::span<const QRgb> bins) {
    m_waterfall->appendColumn(bins);
}

// ================================================================================

void SpectrogramWidget::beginSession(const WaterfallSession& session) {
    m_waterfall->beginSession(session);
}

// ================================================================================

void SpectrogramWidget::liveAdvanced() {
    m_waterfall->liveAdvanced();
}

// ================================================================================

void SpectrogramWidget::redraw() {
    m_waterfall->redraw();
}

// ================================================================================

void SpectrogramWidget::setScale(double secondsPerColumn, double maximumHz, bool logarithmic, double floorHz) {
    m_secondsPerColumn = secondsPerColumn;
    m_maximumHz = maximumHz;
    if (auto* frequency = dynamic_cast<FrequencyAxis*>(m_frequency)) {
        frequency->setRange(maximumHz, logarithmic, floorHz);
    }
}
