#include "AppIcon.h"
#include "MainWindow.h"
#include "ScreenAwake.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("waterfall"));
    QApplication::setApplicationName(QStringLiteral("waterfall"));
    QApplication::setDesktopFileName(QStringLiteral("waterfall"));
    QApplication::setWindowIcon(AppIcon::icon());
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [] { ScreenAwake::setKeepAwake(false); });
    MainWindow window;
    window.show();
    return app.exec();
}
