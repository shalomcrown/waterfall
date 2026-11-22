#include "WaterfallView.h"

#include <QElapsedTimer>
#include <QEventPoint>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QTouchEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr int kFillBudgetMs = 12;
constexpr int kSettleMs = 150;
constexpr double kWheelStep = 1.25;
constexpr double kEpsilon = 1e-9;

long long ceilDivide(long long value, long long divisor) {
    if (value <= 0) {
        return 0;
    }
    return (value + divisor - 1) / divisor;
}
}  // namespace

// ================================================================================

WaterfallView::WaterfallView(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(false);
    setMinimumSize(1, 1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::ClickFocus);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    m_geometry = levelGeometry(0, 0, 1);

    m_fillTimer.setSingleShot(true);
    connect(&m_fillTimer, &QTimer::timeout, this, [this]() { fillMissing(kFillBudgetMs); });
    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(kSettleMs);
    connect(&m_settleTimer, &QTimer::timeout, this, [this]() { commitView(); });

    m_latest = new QPushButton(tr("Latest"), this);
    m_latest->setObjectName(QStringLiteral("latestButton"));
    m_latest->setFocusPolicy(Qt::NoFocus);
    m_latest->setMinimumHeight(36);
    m_latest->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: rgba(24, 34, 52, 220); color: #dfe6ee; "
            "border: 1px solid #5a6b82; border-radius: 6px; padding: 6px 14px; }"
            "QPushButton:pressed { background-color: rgba(60, 80, 112, 230); }"));
    m_latest->hide();
    connect(m_latest, &QPushButton::clicked, this, [this]() { showLatest(); });

    m_info = new QLabel(this);
    m_info->setObjectName(QStringLiteral("zoomInfo"));
    m_info->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_info->setStyleSheet(QStringLiteral(
            "QLabel { background-color: rgba(24, 34, 52, 190); color: #dfe6ee; "
            "border-radius: 4px; padding: 2px 6px; }"));
    m_info->hide();

    m_reset = new QPushButton(tr("Reset zoom"), this);
    m_reset->setObjectName(QStringLiteral("resetButton"));
    m_reset->setFocusPolicy(Qt::NoFocus);
    m_reset->setMinimumHeight(36);
    m_reset->setStyleSheet(m_latest->styleSheet());
    m_reset->hide();
    connect(m_reset, &QPushButton::clicked, this, [this]() { resetView(); });
}

// ================================================================================

QColor WaterfallView::background() {
    return QColor(8, 12, 20);
}

// ================================================================================

int WaterfallView::devicePixelsWide() const {
    return std::max(1, qRound(width() * devicePixelRatioF()));
}

// ================================================================================

int WaterfallView::devicePixelsHigh() const {
    return std::max(1, qRound(height() * devicePixelRatioF()));
}

// ================================================================================

int WaterfallView::slotCount() const {
    const int widthPx = devicePixelsWide();
    const int columnPx = std::max(1, m_geometry.pixelsPerColumn);
    return (widthPx + columnPx - 1) / columnPx;
}

// ================================================================================

int WaterfallView::columnsFilled() const {
    if (m_source == nullptr) {
        return m_manualFilled;
    }
    return static_cast<int>(std::count(m_state.begin(), m_state.end(), static_cast<char>(Drawn)));
}

// ================================================================================

void WaterfallView::clear() {
    if (m_image.isNull() == false) {
        m_image.fill(background());
    }
    m_manualFilled = 0;
    if (m_source != nullptr) {
        std::fill(m_state.begin(), m_state.end(), static_cast<char>(Blank));
        m_fillTimer.stop();
    }
    update();
}

// ================================================================================

void WaterfallView::appendColumn(std::initializer_list<QRgb> bins) {
    appendColumn(std::span<const QRgb>(bins.begin(), bins.size()));
}

// ================================================================================

void WaterfallView::appendColumn(std::span<const QRgb> bins) {
    if (m_source != nullptr || bins.empty() == true || m_image.isNull() == true || m_image.width() < 1 ||
        m_image.height() < 1) {
        return;
    }
    if (m_image.width() >= 2) {
        scrollSlots(1);
    }
    paintSlot(0, bins);
    if (m_manualFilled < m_image.width()) {
        ++m_manualFilled;
    }
    update();
}

// ================================================================================

void WaterfallView::rebuildImage() {
    const int widthPx = devicePixelsWide();
    const int heightPx = devicePixelsHigh();
    int imageWidth = widthPx;
    if (m_source != nullptr) {
        const int slotTotal = slotCount();
        imageWidth = slotTotal * std::max(1, m_geometry.pixelsPerColumn);
        m_state.assign(static_cast<size_t>(slotTotal), static_cast<char>(Missing));
    }
    m_image = QImage(imageWidth, heightPx, QImage::Format_RGB32);
    m_image.fill(background());
    m_manualFilled = 0;
}

// ================================================================================

void WaterfallView::paintBackground(int fromX, int toX) {
    const QRgb color = background().rgb();
    fromX = std::max(0, fromX);
    toX = std::min(m_image.width(), toX);
    for (int y = 0; y < m_image.height(); ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(m_image.scanLine(y));
        for (int x = fromX; x < toX; ++x) {
            row[x] = color;
        }
    }
}

// ================================================================================

void WaterfallView::paintSlot(int slot, std::span<const QRgb> bins) {
    const int columnPx = std::max(1, m_geometry.pixelsPerColumn);
    const int x0 = m_image.width() - (slot + 1) * columnPx;
    const int heightPx = m_image.height();
    const int count = static_cast<int>(bins.size());
    if (x0 < 0 || count < 1 || heightPx < 1) {
        return;
    }
    const double span = m_high - m_low;
    const bool whole = span >= 0.9999 && m_low <= 0.0001;
    for (int y = 0; y < heightPx; ++y) {
        int source = 0;
        if (whole == true) {
            source = y * count / heightPx;
        } else {
            const double fromTop = (1.0 - m_high) + static_cast<double>(y) / static_cast<double>(heightPx) * span;
            source = static_cast<int>(fromTop * static_cast<double>(count) + kEpsilon);
        }
        source = std::clamp(source, 0, count - 1);
        QRgb* row = reinterpret_cast<QRgb*>(m_image.scanLine(y));
        const QRgb color = bins[static_cast<size_t>(source)];
        for (int i = 0; i < columnPx; ++i) {
            row[x0 + i] = color;
        }
    }
}

// ================================================================================

// How many rows a column needs so that the part of it on screen has one row per
// pixel. Columns with more bins than that are pooled to the strongest bin per row.
int WaterfallView::pooledRows() const {
    const double span = std::max(m_high - m_low, kMinFrequencySpan / 2.0);
    return std::max(2, static_cast<int>(std::ceil(static_cast<double>(m_image.height()) / span)));
}

// ================================================================================

void WaterfallView::renderSlot(int slot) {
    const long long column = m_rightColumn - slot;
    const int columnPx = std::max(1, m_geometry.pixelsPerColumn);
    const int x0 = m_image.width() - (slot + 1) * columnPx;
    QVector<QRgb> bins;
    bool drawn = false;
    if (column >= 0) {
        const long long end = gridEnd(column, m_fftSize, m_geometry.hop);
        if (end <= m_source->livePosition()) {
            drawn = m_source->renderColumn(end, m_effectiveFft, pooledRows(), bins);
        }
    }
    if (drawn == true) {
        paintSlot(slot, std::span<const QRgb>(bins.constData(), static_cast<size_t>(bins.size())));
        m_state[static_cast<size_t>(slot)] = Drawn;
        return;
    }
    paintBackground(x0, x0 + columnPx);
    m_state[static_cast<size_t>(slot)] = Blank;
}

// ================================================================================

// Draws the columns that are missing, newest first, for about the given time. What is
// left is finished on a timer so the window stays responsive.
void WaterfallView::fillMissing(int budgetMs) {
    if (m_source == nullptr) {
        return;
    }
    QElapsedTimer timer;
    timer.start();
    bool more = false;
    for (size_t slot = 0; slot < m_state.size(); ++slot) {
        if (m_state[slot] != Missing) {
            continue;
        }
        if (timer.elapsed() >= budgetMs) {
            more = true;
            break;
        }
        renderSlot(static_cast<int>(slot));
    }
    if (more == true) {
        m_fillTimer.start(0);
    } else {
        m_fillTimer.stop();
    }
    update();
}

// ================================================================================

void WaterfallView::invalidateAll() {
    if (m_image.isNull() == false) {
        m_image.fill(background());
    }
    std::fill(m_state.begin(), m_state.end(), static_cast<char>(Missing));
}

// ================================================================================

// Moves the picture by whole columns. A positive delta moves it left, which is what
// new audio does; a negative delta moves it right. Without a session the same code
// shifts the manual columns.
void WaterfallView::scrollSlots(long long delta) {
    if (delta == 0 || m_image.isNull() == true) {
        return;
    }
    const long long slotTotal = m_source == nullptr ? m_image.width() : static_cast<long long>(m_state.size());
    const long long count = delta < 0 ? -delta : delta;
    if (count >= slotTotal) {
        invalidateAll();
        return;
    }
    const int columnPx = m_source == nullptr ? 1 : std::max(1, m_geometry.pixelsPerColumn);
    const int shift = static_cast<int>(count) * columnPx;
    const int widthPx = m_image.width();
    constexpr int bytesPerPixel = 4;
    for (int y = 0; y < m_image.height(); ++y) {
        uchar* row = m_image.scanLine(y);
        if (delta > 0) {
            std::memmove(row, row + shift * bytesPerPixel, static_cast<size_t>(widthPx - shift) * bytesPerPixel);
        } else {
            std::memmove(row + shift * bytesPerPixel, row, static_cast<size_t>(widthPx - shift) * bytesPerPixel);
        }
    }
    if (delta > 0) {
        paintBackground(widthPx - shift, widthPx);
    } else {
        paintBackground(0, shift);
    }
    if (m_source == nullptr) {
        return;
    }
    const size_t size = m_state.size();
    const size_t n = static_cast<size_t>(count);
    if (delta > 0) {
        for (size_t slot = size; slot-- > n;) {
            m_state[slot] = m_state[slot - n];
        }
        std::fill(m_state.begin(), m_state.begin() + static_cast<std::ptrdiff_t>(n), static_cast<char>(Missing));
    } else {
        for (size_t slot = 0; slot + n < size; ++slot) {
            m_state[slot] = m_state[slot + n];
        }
        std::fill(m_state.end() - static_cast<std::ptrdiff_t>(n), m_state.end(), static_cast<char>(Missing));
    }
}

// ================================================================================

void WaterfallView::beginSession(const WaterfallSession& session) {
    m_fillTimer.stop();
    m_settleTimer.stop();
    m_source = session.source;
    m_fftSize = session.fftSize;
    m_baseHop = std::max(1, session.hop);
    m_sampleRate = session.sampleRate;
    m_fromFile = session.fromFile;
    m_clockOrigin = session.clockOriginLocalMs;
    m_level = 0;
    m_effectiveFft = m_fftSize;
    m_geometry = levelGeometry(0, m_fftSize, m_baseHop);
    m_low = 0;
    m_high = 1;
    m_following = true;
    m_rightColumn = -1;
    m_panRemainder = 0;
    resetPreview();
    rebuildImage();
    setCursor(m_source == nullptr ? Qt::ArrowCursor : Qt::OpenHandCursor);
    updateOverlay();
    update();
    emit viewChanged();
}

// ================================================================================

long long WaterfallView::latestColumn() const {
    if (m_source == nullptr) {
        return -1;
    }
    return gridColumnAtOrBefore(m_source->livePosition(), m_fftSize, m_geometry.hop);
}

// ================================================================================

long long WaterfallView::firstColumn() const {
    if (m_source == nullptr) {
        return 0;
    }
    return ceilDivide(m_source->firstPosition(), m_geometry.hop);
}

// ================================================================================

// Keeps the right edge where there is something to see: not past the live edge and
// not so far back that the left edge goes before the oldest sound.
long long WaterfallView::clampRight(long long column) const {
    const long long latest = latestColumn();
    if (latest < 0) {
        return -1;
    }
    const long long oldest = firstColumn() + static_cast<long long>(slotCount()) - 1;
    const long long lowest = std::min(oldest, latest);
    return std::clamp(column, lowest, latest);
}

// ================================================================================

void WaterfallView::moveTo(long long column) {
    if (m_source == nullptr) {
        return;
    }
    const long long target = clampRight(column);
    const long long latest = latestColumn();
    if (target != m_rightColumn) {
        scrollSlots(target - m_rightColumn);
        m_rightColumn = target;
        fillMissing(kFillBudgetMs);
    }
    m_following = latest < 0 || target >= latest;
    updateOverlay();
    update();
    emit viewChanged();
}

// ================================================================================

void WaterfallView::applyGeometry(int level, long long rightColumn, double low, double high, bool follow) {
    m_level = level;
    m_geometry = levelGeometry(level, m_fftSize, m_baseHop);
    m_low = low;
    m_high = high;
    m_effectiveFft = ::effectiveFftSize(m_fftSize, high - low);
    m_rightColumn = clampRight(rightColumn);
    const long long latest = latestColumn();
    m_following = follow == true || latest < 0 || m_rightColumn >= latest;
    resetPreview();
    m_panRemainder = 0;
    rebuildImage();
    fillMissing(kFillBudgetMs);
    updateOverlay();
    update();
    emit viewChanged();
}

// ================================================================================

void WaterfallView::liveAdvanced() {
    if (m_source == nullptr || m_following == false) {
        return;
    }
    const long long latest = latestColumn();
    if (latest >= 0 && latest != m_rightColumn) {
        moveTo(latest);
    }
}

// ================================================================================

void WaterfallView::redraw() {
    if (m_source == nullptr) {
        return;
    }
    commitView();
    invalidateAll();
    fillMissing(kFillBudgetMs);
    update();
}

// ================================================================================

double WaterfallView::samplesPerDevicePixel() const {
    return static_cast<double>(m_geometry.hop) / static_cast<double>(std::max(1, m_geometry.pixelsPerColumn));
}

// ================================================================================

double WaterfallView::rightEdgePosition() const {
    if (m_source == nullptr) {
        return 0.0;
    }
    if (m_rightColumn >= 0) {
        return static_cast<double>(gridEnd(m_rightColumn, m_fftSize, m_geometry.hop));
    }
    return static_cast<double>(m_source->livePosition());
}

// ================================================================================

// The sample position shown at a widget x coordinate, with the stretched copy
// taken into account.
double WaterfallView::positionAtLogicalX(double x) const {
    const double pixelRatio = devicePixelRatioF();
    const double deviceWidth = static_cast<double>(devicePixelsWide());
    const double imageX = (x - m_tx) / m_ts * pixelRatio;
    return rightEdgePosition() - (deviceWidth - imageX) * samplesPerDevicePixel();
}

// ================================================================================

WaterfallTimeWindow WaterfallView::timeWindow() const {
    WaterfallTimeWindow window;
    if (m_source == nullptr || m_sampleRate < 1) {
        return window;
    }
    window.valid = true;
    window.clock = m_fromFile == false;
    window.originLocalMs = m_clockOrigin;
    window.leftSeconds = positionAtLogicalX(0.0) / static_cast<double>(m_sampleRate);
    window.rightSeconds = positionAtLogicalX(static_cast<double>(width())) / static_cast<double>(m_sampleRate);
    return window;
}

// ================================================================================

WaterfallFrequencyWindow WaterfallView::frequencyWindow() const {
    WaterfallFrequencyWindow window;
    const double span = m_high - m_low;
    const double heightLogical = std::max(1.0, static_cast<double>(height()));
    window.high = m_high + m_ty / (m_fs * heightLogical) * span;
    window.low = m_high - span * (heightLogical - m_ty) / (m_fs * heightLogical);
    return window;
}

// ================================================================================

bool WaterfallView::previewActive() const {
    return std::abs(m_ts - 1.0) > kEpsilon || std::abs(m_fs - 1.0) > kEpsilon || std::abs(m_tx) > kEpsilon ||
           std::abs(m_ty) > kEpsilon;
}

// ================================================================================

void WaterfallView::resetPreview() {
    m_tx = 0;
    m_ts = 1;
    m_ty = 0;
    m_fs = 1;
}

// ================================================================================

// Keeps the stretched copy inside the frequency axis and within the zoom limits.
void WaterfallView::clampFrequencyPreview() {
    const double span = m_high - m_low;
    if (span <= 0.0) {
        m_ty = 0;
        m_fs = 1;
        return;
    }
    m_fs = std::clamp(m_fs, span, span / kMinFrequencySpan);
    const double heightLogical = std::max(1.0, static_cast<double>(height()));
    const WaterfallFrequencyWindow window = frequencyWindow();
    const double perPixel = span / (m_fs * heightLogical);
    if (window.high > 1.0) {
        m_ty -= (window.high - 1.0) / perPixel;
    }
    const WaterfallFrequencyWindow moved = frequencyWindow();
    if (moved.low < 0.0) {
        m_ty += (0.0 - moved.low) / perPixel;
    }
}

// ================================================================================

// Limits a zoom factor so the time zoom stays on the ladder.
double WaterfallView::timeFactorLimit(double factor) const {
    const double most = std::pow(2.0, static_cast<double>(m_level - minimumZoomLevel(m_fftSize, m_baseHop)) / 2.0);
    const double least = std::pow(2.0, -static_cast<double>(kMaxZoomOutLevel - m_level) / 2.0);
    const double target = std::clamp(m_ts * factor, least, most);
    return target / m_ts;
}

// ================================================================================

void WaterfallView::zoomBy(double timeFactor, double frequencyFactor, QPointF anchor) {
    if (m_source == nullptr || m_fftSize < 2) {
        return;
    }
    if (std::abs(timeFactor - 1.0) > kEpsilon) {
        const double factor = timeFactorLimit(timeFactor);
        m_tx = anchor.x() - (anchor.x() - m_tx) * factor;
        m_ts *= factor;
    }
    if (std::abs(frequencyFactor - 1.0) > kEpsilon) {
        const double span = m_high - m_low;
        const double target = std::clamp(m_fs * frequencyFactor, span, span / kMinFrequencySpan);
        const double factor = target / m_fs;
        m_ty = anchor.y() - (anchor.y() - m_ty) * factor;
        m_fs = target;
        clampFrequencyPreview();
    }
    update();
    emit viewChanged();
}

// ================================================================================

void WaterfallView::previewPanBy(double dxPixels, double dyPixels) {
    if (m_source == nullptr) {
        return;
    }
    m_tx += dxPixels;
    m_ty += dyPixels;
    clampFrequencyPreview();
    update();
    emit viewChanged();
}

// ================================================================================

void WaterfallView::panBy(double dxPixels, double dyPixels) {
    if (m_source == nullptr) {
        return;
    }
    if (std::abs(m_ts - 1.0) > kEpsilon || std::abs(m_tx) > kEpsilon) {
        previewPanBy(dxPixels, dyPixels);
        return;
    }
    const double columnWidth = static_cast<double>(std::max(1, m_geometry.pixelsPerColumn)) / devicePixelRatioF();
    const double total = m_panRemainder + dxPixels / columnWidth;
    const double whole = std::trunc(total);
    m_panRemainder = total - whole;
    if (whole != 0.0) {
        moveTo(m_rightColumn - static_cast<long long>(whole));
    }
    if (dyPixels != 0.0) {
        previewPanBy(0.0, dyPixels);
    }
}

// ================================================================================

// Turns the stretched copy into a real picture: picks the nearest zoom on the
// ladder, and draws only what is not already drawn.
void WaterfallView::commitView() {
    m_settleTimer.stop();
    if (previewActive() == false) {
        return;
    }
    if (m_source == nullptr || m_fftSize < 2) {
        resetPreview();
        update();
        return;
    }
    const double rightPosition = positionAtLogicalX(static_cast<double>(width()));
    const bool timeScaled = std::abs(m_ts - 1.0) > kEpsilon;
    const int level = timeScaled == true ? levelAfterScale(m_level, m_ts, m_fftSize, m_baseHop) : m_level;
    const LevelGeometry geometry = levelGeometry(level, m_fftSize, m_baseHop);
    const long long column = std::llround((rightPosition - static_cast<double>(m_fftSize)) /
                                          static_cast<double>(geometry.hop));
    WaterfallFrequencyWindow window = frequencyWindow();
    window.low = std::clamp(window.low, 0.0, 1.0);
    window.high = std::clamp(window.high, 0.0, 1.0);
    const bool frequencyMoved = std::abs(window.low - m_low) > 1e-6 || std::abs(window.high - m_high) > 1e-6;
    if (level == m_level && frequencyMoved == false) {
        resetPreview();
        moveTo(column);
        return;
    }
    applyGeometry(level, column, window.low, window.high, false);
}

// ================================================================================

void WaterfallView::showLatest() {
    commitView();
    moveTo(latestColumn());
}

// ================================================================================

void WaterfallView::resetView() {
    if (m_source == nullptr) {
        return;
    }
    m_settleTimer.stop();
    applyGeometry(0, latestColumn(), 0.0, 1.0, true);
}

// ================================================================================

double WaterfallView::effectiveOverlapPercent() const {
    return overlapPercentOf(m_geometry.hop, m_effectiveFft);
}

// ================================================================================

void WaterfallView::updateOverlay() {
    updateZoomInfo();
    if (m_latest == nullptr) {
        return;
    }
    const bool visible = m_source != nullptr && m_following == false;
    if (visible == true) {
        m_latest->adjustSize();
        m_latest->move(std::max(0, width() - m_latest->width() - 8), 8);
        m_latest->raise();
    }
    m_latest->setVisible(visible);
}

// ================================================================================

// The overlay button under a touch, if it is showing.
QPushButton* WaterfallView::overlayButtonAt(const QPointF& point) const {
    for (QPushButton* button : {m_latest, m_reset}) {
        if (button != nullptr && button->isVisible() == true && button->geometry().contains(point.toPoint())) {
            return button;
        }
    }
    return nullptr;
}

// ================================================================================

bool WaterfallView::isDefaultZoom() const {
    return m_level == 0 && m_effectiveFft == m_fftSize && m_high - m_low >= 0.999;
}

// ================================================================================

// Always says what the picture is drawn with, because zooming changes the FFT length
// and the overlap by itself. The reset button shows while the zoom is not the
// configured one.
void WaterfallView::updateZoomInfo() {
    if (m_info == nullptr || m_reset == nullptr) {
        return;
    }
    if (m_source == nullptr || m_sampleRate < 1 || m_effectiveFft < 2) {
        m_info->hide();
        m_reset->hide();
        return;
    }
    const double binHz = static_cast<double>(m_sampleRate) / static_cast<double>(m_effectiveFft);
    const QString bin = binHz >= 10.0 ? QString::number(binHz, 'f', 0) : QString::number(binHz, 'f', 1);
    m_info->setText(tr("FFT %1 · %2 Hz bins · overlap %3%")
                            .arg(m_effectiveFft)
                            .arg(bin)
                            .arg(QString::number(effectiveOverlapPercent(), 'f', 1)));
    m_info->adjustSize();
    m_info->move(8, 8);
    m_info->raise();
    m_info->show();
    const bool zoomed = isDefaultZoom() == false;
    if (zoomed == true) {
        m_reset->adjustSize();
        m_reset->move(8, m_info->y() + m_info->height() + 6);
        m_reset->raise();
    }
    m_reset->setVisible(zoomed);
}

// ================================================================================

void WaterfallView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), background());
    if (m_image.isNull() == true) {
        return;
    }
    const int visibleWidth = std::min(devicePixelsWide(), m_image.width());
    const QRectF source(m_image.width() - visibleWidth, 0, visibleWidth, m_image.height());
    if (previewActive() == false) {
        painter.drawImage(QRectF(rect()), m_image, source);
        return;
    }
    painter.drawImage(QRectF(m_tx, m_ty, width() * m_ts, height() * m_fs), m_image, source);
}

// ================================================================================

void WaterfallView::resizeEvent(QResizeEvent*) {
    if (m_source != nullptr) {
        m_settleTimer.stop();
        resetPreview();
        if (m_following == true) {
            m_rightColumn = latestColumn();
        } else {
            m_rightColumn = clampRight(m_rightColumn);
        }
    }
    rebuildImage();
    fillMissing(kFillBudgetMs);
    updateOverlay();
    update();
    emit viewChanged();
}

// ================================================================================

void WaterfallView::mousePressEvent(QMouseEvent* event) {
    if (m_source == nullptr || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    commitView();
    m_dragging = true;
    m_lastMouse = event->position();
    m_panRemainder = 0;
    setCursor(Qt::ClosedHandCursor);
    setFocus();
    event->accept();
}

// ================================================================================

void WaterfallView::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging == false) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPointF delta = event->position() - m_lastMouse;
    m_lastMouse = event->position();
    panBy(delta.x(), delta.y());
    event->accept();
}

// ================================================================================

void WaterfallView::mouseReleaseEvent(QMouseEvent* event) {
    if (m_dragging == false) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    m_dragging = false;
    setCursor(Qt::OpenHandCursor);
    commitView();
    event->accept();
}

// ================================================================================

void WaterfallView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (m_source == nullptr || event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    resetView();
    event->accept();
}

// ================================================================================

void WaterfallView::wheelEvent(QWheelEvent* event) {
    if (m_source == nullptr) {
        event->ignore();
        return;
    }
    const QPoint angle = event->angleDelta();
    const int delta = angle.y() != 0 ? angle.y() : angle.x();
    if (delta == 0) {
        event->ignore();
        return;
    }
    const double notches = static_cast<double>(delta) / 120.0;
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    const bool control = modifiers.testFlag(Qt::ControlModifier);
    const bool shift = modifiers.testFlag(Qt::ShiftModifier);
    if (control == true) {
        const double factor = std::pow(kWheelStep, notches);
        if (shift == true) {
            zoomBy(1.0, factor, event->position());
        } else {
            zoomBy(factor, 1.0, event->position());
        }
        m_settleTimer.start();
    } else {
        commitView();
        m_panRemainder = 0;
        panBy(notches * static_cast<double>(width()) * 0.1, 0.0);
    }
    event->accept();
}

// ================================================================================

void WaterfallView::keyPressEvent(QKeyEvent* event) {
    if (m_source == nullptr) {
        QWidget::keyPressEvent(event);
        return;
    }
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    const QPointF center(width() / 2.0, height() / 2.0);
    const double step = std::sqrt(2.0);
    commitView();
    switch (event->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomBy(shift == true ? 1.0 : step, shift == true ? step : 1.0, center);
        commitView();
        break;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        zoomBy(shift == true ? 1.0 : 1.0 / step, shift == true ? 1.0 / step : 1.0, center);
        commitView();
        break;
    case Qt::Key_Left:
        m_panRemainder = 0;
        panBy(width() * 0.1, 0.0);
        break;
    case Qt::Key_Right:
        m_panRemainder = 0;
        panBy(-width() * 0.1, 0.0);
        break;
    case Qt::Key_Up:
        previewPanBy(0.0, height() * 0.1);
        commitView();
        break;
    case Qt::Key_Down:
        previewPanBy(0.0, -height() * 0.1);
        commitView();
        break;
    case Qt::Key_Home:
        resetView();
        break;
    case Qt::Key_End:
        showLatest();
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

// ================================================================================

bool WaterfallView::event(QEvent* event) {
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        if (m_source != nullptr) {
            handleTouch(static_cast<QTouchEvent*>(event));
            event->accept();
            return true;
        }
        break;
    case QEvent::NativeGesture: {
        auto* gesture = static_cast<QNativeGestureEvent*>(event);
        if (m_source != nullptr && gesture->gestureType() == Qt::ZoomNativeGesture) {
            zoomBy(1.0 + gesture->value(), 1.0, gesture->position());
            m_settleTimer.start();
            event->accept();
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QWidget::event(event);
}

// ================================================================================

// One finger pans. Two fingers zoom by the change in their distance: a pinch that is
// mostly sideways zooms time, mostly up and down zooms frequency, and a diagonal
// one zooms both. The movement of the middle between the fingers pans.
void WaterfallView::handleTouch(QTouchEvent* event) {
    std::vector<QPointF> points;
    if (event->type() != QEvent::TouchCancel) {
        for (const QEventPoint& point : event->points()) {
            if (point.state() != QEventPoint::State::Released) {
                points.push_back(point.position());
            }
        }
    }
    touchPoints(points);
}

// ================================================================================

void WaterfallView::touchPoints(const std::vector<QPointF>& points) {
    if (m_source == nullptr) {
        return;
    }
    const int count = static_cast<int>(points.size());
    QPointF centroid;
    double distance = 0.0;
    if (count == 1) {
        centroid = points[0];
    } else if (count >= 2) {
        centroid = (points[0] + points[1]) / 2.0;
        distance = std::hypot(points[1].x() - points[0].x(), points[1].y() - points[0].y());
    }

    if (count != m_touchCount) {
        QPushButton* const pressed = m_touchButton;
        if (m_touchCount > 0 && pressed == nullptr) {
            commitView();
        }
        if (count == 0) {
            const bool inside = pressed != nullptr && overlayButtonAt(m_touchCentroid) == pressed;
            m_touchButton = nullptr;
            m_touchCount = 0;
            if (inside == true && pressed == m_latest) {
                showLatest();
            } else if (inside == true && pressed == m_reset) {
                resetView();
            }
            return;
        }
        if (m_touchCount == 0 && count == 1) {
            m_touchButton = overlayButtonAt(centroid);
        }
        m_touchCount = count;
        m_touchCentroid = centroid;
        m_touchDistance = distance;
        m_panRemainder = 0;
        m_pinchAxes = PinchAxes::Both;
        if (count >= 2) {
            const double across = std::abs(points[1].x() - points[0].x());
            const double up = std::abs(points[1].y() - points[0].y());
            const double angle = std::atan2(up, across) * 180.0 / 3.14159265358979323846;
            if (angle < 30.0) {
                m_pinchAxes = PinchAxes::TimeOnly;
            } else if (angle > 60.0) {
                m_pinchAxes = PinchAxes::FrequencyOnly;
            }
        }
        return;
    }

    if (m_touchButton != nullptr) {
        m_touchCentroid = centroid;
        return;
    }
    if (count == 1) {
        const QPointF delta = centroid - m_touchCentroid;
        panBy(delta.x(), delta.y());
    } else if (count >= 2) {
        constexpr double minimumDistance = 8.0;
        if (m_touchDistance >= minimumDistance && distance >= minimumDistance) {
            const double factor = distance / m_touchDistance;
            const double timeFactor = m_pinchAxes == PinchAxes::FrequencyOnly ? 1.0 : factor;
            const double frequencyFactor = m_pinchAxes == PinchAxes::TimeOnly ? 1.0 : factor;
            zoomBy(timeFactor, frequencyFactor, m_touchCentroid);
        }
        const QPointF delta = centroid - m_touchCentroid;
        previewPanBy(delta.x(), delta.y());
    }
    m_touchCentroid = centroid;
    m_touchDistance = distance;
}
