#include "AppIcon.h"

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QWidget>

namespace {
QPixmap drawIcon(int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal radius = size * 0.22;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(8, 12, 20));
    painter.drawRoundedRect(QRectF(0, 0, size, size), radius, radius);

    const QColor columns[] = {QColor(59, 82, 139), QColor(33, 145, 140), QColor(94, 201, 98),
                              QColor(253, 231, 37), QColor(94, 201, 98),  QColor(33, 145, 140),
                              QColor(253, 231, 37), QColor(59, 82, 139)};
    const qreal margin = size * 0.14;
    const qreal gap = size * 0.03;
    const qreal bar = (size - margin * 2 - gap * 7) / 8.0;
    const qreal top = size * 0.22;
    const qreal height = size * 0.56;
    for (int i = 0; i < 8; ++i) {
        painter.setBrush(columns[i]);
        painter.drawRoundedRect(QRectF(margin + i * (bar + gap), top, bar, height), size * 0.02, size * 0.02);
    }
    return pixmap;
}
}  // namespace

// ================================================================================

QIcon AppIcon::icon() {
    QIcon icon;
    icon.addPixmap(drawIcon(64));
    icon.addPixmap(drawIcon(128));
    icon.addPixmap(drawIcon(256));
    return icon;
}

// ================================================================================

void AppIcon::apply(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }
    widget->setWindowIcon(icon());
}
