#include "WaterfallView.h"

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

#include <algorithm>
#include <cstring>

WaterfallView::WaterfallView(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(false);
    setMinimumSize(1, 1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

// ================================================================================

QColor WaterfallView::background() {
    return QColor(8, 12, 20);
}

// ================================================================================

void WaterfallView::appendColumn(std::initializer_list<QRgb> bins) {
    appendColumn(std::span<const QRgb>(bins.begin(), bins.size()));
}

// ================================================================================

void WaterfallView::appendColumn(std::span<const QRgb> bins) {
    if (bins.empty() == true || m_image.isNull() == true || m_image.width() < 1 || m_image.height() < 1) {
        return;
    }
    if (m_image.width() >= 2) {
        shiftLeft();
    }
    paintColumn(m_image.width() - 1, bins);
    if (m_filled < m_image.width()) {
        ++m_filled;
    }
    update();
}

// ================================================================================

void WaterfallView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), background());
    if (m_image.isNull() == false) {
        painter.drawImage(rect(), m_image);
    }
}

// ================================================================================

void WaterfallView::resizeEvent(QResizeEvent*) {
    rebuildImage();
}

// ================================================================================

void WaterfallView::rebuildImage() {
    const qreal dpr = devicePixelRatioF();
    const int widthPx = std::max(1, qRound(width() * dpr));
    const int heightPx = std::max(1, qRound(height() * dpr));
    m_image = QImage(widthPx, heightPx, QImage::Format_RGB32);
    m_image.setDevicePixelRatio(dpr);
    m_image.fill(background());
    m_filled = 0;
    update();
}

// ================================================================================

void WaterfallView::paintColumn(int x, std::span<const QRgb> bins) {
    const int heightPx = m_image.height();
    const int sourceCount = static_cast<int>(bins.size());
    for (int y = 0; y < heightPx; ++y) {
        const int source = y * sourceCount / heightPx;
        m_image.setPixel(x, y, bins[static_cast<size_t>(source)]);
    }
}

// ================================================================================

void WaterfallView::shiftLeft() {
    const int widthPx = m_image.width();
    const int heightPx = m_image.height();
    if (widthPx < 2) {
        return;
    }
    constexpr int bytesPerPixel = 4;
    const int rowBytes = (widthPx - 1) * bytesPerPixel;
    for (int y = 0; y < heightPx; ++y) {
        uchar* row = m_image.scanLine(y);
        std::memmove(row, row + bytesPerPixel, static_cast<size_t>(rowBytes));
    }
}
