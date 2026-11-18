#include "AppIcon.h"
#include "MainWindow.h"
#include "ScreenAwake.h"

#include <QApplication>

#include <cstdio>
#include <cstring>

extern const char* VERSION;

int main(int argc, char* argv[]) {
    if (argc == 2 && (std::strcmp(argv[1], "--version") == 0 || std::strcmp(argv[1], "-v") == 0)) {
        std::puts(VERSION);
        return 0;
    }

    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("waterfall"));
    QApplication::setApplicationName(QStringLiteral("waterfall"));
    QApplication::setApplicationVersion(QString::fromUtf8(VERSION));
    QApplication::setDesktopFileName(QStringLiteral("waterfall"));
    QApplication::setWindowIcon(AppIcon::icon());
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [] { ScreenAwake::setKeepAwake(false); });
    MainWindow window;
    window.show();
    return app.exec();
}
