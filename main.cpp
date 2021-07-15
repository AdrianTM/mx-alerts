#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include "mainwindow.h"
#include "version.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationVersion(VERSION);
    app.setOrganizationName("MX-Linux");

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Displays alerts and urgent notifications from MX Linux team"));
    parser.addVersionOption();
    parser.addHelpOption();
    parser.addOption({{"b", "batch"}, QObject::tr("Run program manually, check for updates and exit.")});
    parser.process(app);

    if (!parser.isSet("batch") && system("ps -C mx-alerts --no-headers |sed -n '2p' |grep mx-alerts") == 0) {
        qDebug("mx-alerts already running, exiting...");
        return EXIT_FAILURE;
    }

    app.setWindowIcon(QIcon::fromTheme("info"));
    QApplication::setQuitOnLastWindowClosed(false);

    QTranslator qtTran;
    if (qtTran.load(QLocale::system(), "qt", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTran);

    QTranslator qtBaseTran;
    if (qtBaseTran.load("qtbase_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtBaseTran);

    QTranslator appTran;
    if (appTran.load(app.applicationName() + "_" + QLocale::system().name(), "/usr/share/" + app.applicationName() + "/locale"))
        app.installTranslator(&appTran);

    MainWindow w(parser);
    w.hide();
    return app.exec();
}
