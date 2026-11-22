#pragma once

#include <QRgb>
#include <QWidget>

#include <span>

class WaterfallView;
struct WaterfallSession;

// Waterfall plus a frequency scale on the left and a time scale below. Both scales
// follow what the waterfall currently shows.
class SpectrogramWidget : public QWidget {
public:
    explicit SpectrogramWidget(QWidget* parent = nullptr);

    WaterfallView* waterfall() const { return m_waterfall; }
    // Without a session: columns are handed in one by one.
    void appendColumn(std::span<const QRgb> bins);
    // With a session: the waterfall draws from the sound buffer.
    void beginSession(const WaterfallSession& session);
    void liveAdvanced();
    void redraw();
    void setScale(double secondsPerColumn, double maximumHz, bool logarithmic = false, double floorHz = 5);
    double maximumHz() const { return m_maximumHz; }
    double secondsPerColumn() const { return m_secondsPerColumn; }

private:
    WaterfallView* m_waterfall = nullptr;
    QWidget* m_frequency = nullptr;
    QWidget* m_time = nullptr;
    double m_secondsPerColumn = 0;
    double m_maximumHz = 0;
};
