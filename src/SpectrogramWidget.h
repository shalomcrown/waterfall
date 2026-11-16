#pragma once

#include <QRgb>
#include <QWidget>

#include <span>

class WaterfallView;

// Waterfall plus a frequency scale on the left and a scrolling time scale below.
class SpectrogramWidget : public QWidget {
public:
    explicit SpectrogramWidget(QWidget* parent = nullptr);

    WaterfallView* waterfall() const { return m_waterfall; }
    void appendColumn(std::span<const QRgb> bins);
    void setScale(double secondsPerColumn, double maximumHz);
    double maximumHz() const { return m_maximumHz; }
    double secondsPerColumn() const { return m_secondsPerColumn; }

private:
    WaterfallView* m_waterfall = nullptr;
    QWidget* m_frequency = nullptr;
    QWidget* m_time = nullptr;
    double m_secondsPerColumn = 0;
    double m_maximumHz = 0;
};
