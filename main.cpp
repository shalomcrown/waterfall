#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("waterfall"));
    QApplication::setApplicationName(QStringLiteral("waterfall"));
    MainWindow window;
    window.show();
    return app.exec();
}
