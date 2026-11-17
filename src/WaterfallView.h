#pragma once

#include <QImage>
#include <QRgb>
#include <QWidget>

#include <initializer_list>
#include <span>

// History of vertical color columns. Each new column is painted at the
// right edge and the older columns shift left immediately.
class WaterfallView : public QWidget {
    Q_OBJECT

public:
    explicit WaterfallView(QWidget* parent = nullptr);

    void appendColumn(std::span<const QRgb> bins);
    void appendColumn(std::initializer_list<QRgb> bins);
    void clear();
    const QImage& image() const { return m_image; }
    int columnsFilled() const { return m_filled; }
    static QColor background();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildImage();
    void paintColumn(int x, std::span<const QRgb> bins);
    void shiftLeft();

    QImage m_image;
    int m_filled = 0;
};
