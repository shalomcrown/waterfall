#pragma once

class QIcon;
class QWidget;

class AppIcon {
public:
    static QIcon icon();
    static void apply(QWidget* widget);
};
