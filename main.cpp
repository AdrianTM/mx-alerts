#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    if (system("ps -C mx-alerts --no-headers | sed -n '2p' | grep mx-alerts") == 0) {
        return EXIT_FAILURE;
    }
    QApplication app(argc, argv);

    if (app.arguments().contains("--version") || app.arguments().contains("-v") ) {
       qDebug() << "Version:" << VERSION;
       return EXIT_SUCCESS;
    }

    app.setWindowIcon(QIcon::fromTheme("info"));
    QApplication::setQuitOnLastWindowClosed(false);

    QTranslator qtTran;
    qtTran.load(QString("qt_") + QLocale::system().name());
    app.installTranslator(&qtTran);

    QTranslator appTran;
    appTran.load(QString("mx-boot-options_") + QLocale::system().name(), "/usr/share/mx-alerts/locale");
    app.installTranslator(&appTran);

    MainWindow window(app.arguments());
    window.hide();
    return app.exec();
}
