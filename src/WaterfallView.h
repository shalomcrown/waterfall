#pragma once

#include "ColumnSource.h"
#include "ViewTiming.h"

#include <QImage>
#include <QPointF>
#include <QRgb>
#include <QTimer>
#include <QWidget>

#include <initializer_list>
#include <span>
#include <vector>

class QLabel;
class QPushButton;
class QTouchEvent;

// What a session looks like to the view. Positions are sample positions counted
// from the start of the session.
struct WaterfallSession {
    const ColumnSource* source = nullptr;
    int fftSize = 0;
    int hop = 0;
    int sampleRate = 0;
    // A file shows the position in the file; live capture shows the clock.
    bool fromFile = false;
    // Wall-clock time of sample position 0, in milliseconds since the epoch with the
    // local UTC offset added. Only used for live capture.
    long long clockOriginLocalMs = 0;
};

// The span of time the view shows, for the time axis.
struct WaterfallTimeWindow {
    bool valid = false;
    bool clock = false;
    double leftSeconds = 0;
    double rightSeconds = 0;
    long long originLocalMs = 0;
};

// The part of the frequency axis the view shows, as fractions of the whole axis
// from the bottom (0) to the top (1). Logarithmic and linear axes both use it.
struct WaterfallFrequencyWindow {
    double low = 0;
    double high = 1;
};

// A scrolling waterfall. With a session the view draws from the sound buffer, so
// any part of it can be redrawn at another zoom: only the columns that are not
// already drawn are computed. Without a session it just holds the columns that
// appendColumn gives it, and shifts them left.
class WaterfallView : public QWidget {
    Q_OBJECT

public:
    explicit WaterfallView(QWidget* parent = nullptr);

    // Without a session.
    void appendColumn(std::span<const QRgb> bins);
    void appendColumn(std::initializer_list<QRgb> bins);
    void clear();

    // With a session. beginSession starts over: the view follows the live edge
    // at the base zoom.
    void beginSession(const WaterfallSession& session);
    bool hasSession() const { return m_source != nullptr; }
    // Call when new audio arrived. A following view scrolls to the newest column.
    void liveAdvanced();
    // Draws everything again from the sound buffer, for example after the
    // frequency scale changed.
    void redraw();

    // Zoom and pan. Time pans commit as they go. Zooms and frequency pans are
    // shown as a stretched copy of the picture until commitView().
    void panBy(double dxPixels, double dyPixels);
    void previewPanBy(double dxPixels, double dyPixels);
    void zoomBy(double timeFactor, double frequencyFactor, QPointF anchor);
    void commitView();
    // The fingers now on the screen, in widget coordinates; an empty list means all
    // fingers are up. This is what touch events are turned into.
    void touchPoints(const std::vector<QPointF>& points);
    void showLatest();
    void resetView();

    bool following() const { return m_following; }
    int zoomLevel() const { return m_level; }
    int columnPixels() const { return m_geometry.pixelsPerColumn; }
    // The FFT length the picture is drawn with. Zooming in on frequency lengthens it.
    int effectiveFftSize() const { return m_effectiveFft; }
    // The overlap of the picture's windows, which rises as the zoom gets deeper.
    double effectiveOverlapPercent() const;
    QLabel* zoomInfo() const { return m_info; }
    bool previewActive() const;
    WaterfallTimeWindow timeWindow() const;
    WaterfallFrequencyWindow frequencyWindow() const;
    QPushButton* latestButton() const { return m_latest; }
    QPushButton* resetButton() const { return m_reset; }
    // True when the picture is at the configured zoom: the capture FFT and overlap,
    // the whole frequency axis, and the first time level.
    bool isDefaultZoom() const;

    const QImage& image() const { return m_image; }
    int columnsFilled() const;
    static QColor background();

signals:
    void viewChanged();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum SlotState : char { Missing = 0, Blank = 1, Drawn = 2 };
    enum class PinchAxes { Both, TimeOnly, FrequencyOnly };

    int devicePixelsWide() const;
    int devicePixelsHigh() const;
    int slotCount() const;
    void rebuildImage();
    void paintBackground(int fromX, int toX);
    void paintSlot(int slot, std::span<const QRgb> bins);
    void renderSlot(int slot);
    void fillMissing(int budgetMs);
    void invalidateAll();
    long long latestColumn() const;
    long long firstColumn() const;
    long long clampRight(long long column) const;
    void moveTo(long long column);
    void applyGeometry(int level, long long rightColumn, double low, double high, bool follow);
    void scrollSlots(long long delta);
    double rightEdgePosition() const;
    double samplesPerDevicePixel() const;
    double positionAtLogicalX(double x) const;
    void clampFrequencyPreview();
    void resetPreview();
    void updateOverlay();
    void updateZoomInfo();
    QPushButton* overlayButtonAt(const QPointF& point) const;
    int pooledRows() const;
    void handleTouch(QTouchEvent* event);
    double timeFactorLimit(double factor) const;

    // Without a session.
    QImage m_image;
    int m_manualFilled = 0;

    // Session.
    const ColumnSource* m_source = nullptr;
    int m_fftSize = 0;
    int m_baseHop = 1;
    int m_sampleRate = 0;
    bool m_fromFile = false;
    long long m_clockOrigin = 0;

    // What the picture holds.
    int m_level = 0;
    int m_effectiveFft = 0;
    LevelGeometry m_geometry;
    long long m_rightColumn = -1;
    bool m_following = true;
    double m_low = 0;
    double m_high = 1;
    std::vector<char> m_state;

    // The stretched copy shown while a gesture is in progress: a point at x
    // is drawn at tx + x * ts, and likewise y with ty and fs.
    double m_tx = 0;
    double m_ts = 1;
    double m_ty = 0;
    double m_fs = 1;
    double m_panRemainder = 0;

    QTimer m_fillTimer;
    QTimer m_settleTimer;
    QPushButton* m_latest = nullptr;
    QLabel* m_info = nullptr;
    QPushButton* m_reset = nullptr;

    bool m_dragging = false;
    QPointF m_lastMouse;

    int m_touchCount = 0;
    QPointF m_touchCentroid;
    double m_touchDistance = 0;
    PinchAxes m_pinchAxes = PinchAxes::Both;
    QPushButton* m_touchButton = nullptr;
};
