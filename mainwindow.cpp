#include <QtGui>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "mainwindow.h"

MainWindow::MainWindow(const QCommandLineParser &arg_parser)
{
    createActions();
    loadSettings();
    createMenu();

    connect(alertIcon, &QSystemTrayIcon::messageClicked, this, &MainWindow::messageClicked);
    connect(alertIcon, &QSystemTrayIcon::activated, this, &MainWindow::iconActivated);
    connect(&timer, &QTimer::timeout, this, &MainWindow::checkUpdates);

    QDir dir;
    dir.mkdir(tmpFolder);

    setIcon("info");
    release = getCmdOut("grep -oP '(?<=DISTRIB_RELEASE=).*' /etc/lsb-release").str;

    if (arg_parser.isSet("batch")) { // check for updates and exit when running --batch
        if (!checkUpdates())
            QTimer::singleShot(0, qApp, &QGuiApplication::quit);
    } else {
        alertIcon->show();
        checkUpdates();
    }
    setWindowTitle(tr("MX Alerts Preferences"));
    this->adjustSize();
}

// util function for getting bash command output and error code
Output MainWindow::getCmdOut(const QString &cmd)
{
    qDebug() << cmd;
    QProcess proc;
    QEventLoop loop;
    proc.setReadChannelMode(QProcess::MergedChannels);
    proc.start("/bin/bash", QStringList() << "-c" << cmd);
    proc.waitForFinished();
    qDebug() << "exitCode" << proc.exitCode();
    return {proc.exitCode(), proc.readAll().trimmed()};
}

// Custom sleep
void MainWindow::csleep(int msec)
{
    QTimer cstimer(this);
    QEventLoop eloop;
    connect(&cstimer, &QTimer::timeout, &eloop, &QEventLoop::quit);
    cstimer.start(msec);
    eloop.exec();
}

void MainWindow::setIcon(QString icon_name)
{
    alertIcon->setIcon(QIcon::fromTheme(icon_name));
    setWindowIcon(QIcon::fromTheme(icon_name));
}

bool MainWindow::showLastAlert(bool clicked)
{
    QString fileName = "alert" + release;
    if (!verifySignature()) {
        qDebug() << "Bad or missing signature";
        QFile::remove(tmpFolder + fileName);
        QFile::remove(tmpFolder + fileName + ".sig");
        userSettings.remove("LastAlert");
        return false;
    } else {
        if (!clicked)
            setIcon("messagebox_critical");
        displayFile(fileName, clicked);
    }
    return true;
}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
        showLastAlert(true);
        break;
    case QSystemTrayIcon::DoubleClick:
        showNormal();
        raise();
        break;
    case QSystemTrayIcon::MiddleClick:
        showNormal();
        raise();
        break;
    default:
        break;
    }
}

void MainWindow::loadSettings()
{
    QSettings settings("/etc/mx-alerts.conf", QSettings::IniFormat);
    server = settings.value("Server").toString();
    autoStartup = settings.value("AutoStartup").toBool();

    // load user settings if present, otherwise use system settings
    server = userSettings.value("Server", server).toString();
    autoStartup = userSettings.value("AutoStartup", autoStartup).toBool();
    setEnabled(autoStartup);

    qDebug() << "Server" << server;
}

void MainWindow::toggleDisabled()
{
    if (!autoStartup) {
        getCmdOut("(crontab -l; echo '0 */5 * * * sleep $(( $(od -N2 -tu2 -An /dev/urandom) \\% 3600 )); /usr/bin/mx-alerts.sh') |crontab -");
        QMessageBox::information(this, tr("Startup enabled"),
                                 tr("You enabled automatic startup for the alerts program. "
                                    "The program will check priodically for alerts"));
        setEnabled(true);
    } else {
        getCmdOut("crontab -l |sed '\\;/usr/bin/mx-alerts.sh;d' |crontab -");
        QMessageBox::information(this, tr("Startup disabled"),
                                 tr("You disabled automatic startup for the alerts program.\n"
                                    "Please start the program manually from the menu to check updates."));

        setEnabled(false);
    }
    userSettings.setValue("AutoStartup", autoStartup);
}

void MainWindow::setEnabled(bool enabled)
{
    autoStartup = enabled;
    if (enabled) {
        toggleDisableAction->setIcon(QIcon::fromTheme("notification-disabled-symbolic"));
        toggleDisableAction->setText(tr("&Disable autostart"));
    } else {
        toggleDisableAction->setIcon(QIcon::fromTheme("notification-symbolic"));
        toggleDisableAction->setText(tr("&Enable autostart"));
    }
}

void MainWindow::showMessage(QString title, QString body)
{
    alertIcon->showMessage(title, body, QSystemTrayIcon::Critical, 0);
}

void MainWindow::writeFile(QString extension)
{
    QString fileName = "alert" + release;
    QFile file(tmpFolder + fileName + extension);
    if (!file.open(QFile::WriteOnly)) {
        qDebug() << "Could not open file:" << file.fileName();
        QTimer::singleShot(0, qApp, &QGuiApplication::quit);
    }
    file.write(reply->readAll());
    file.close();
}

QString MainWindow::getDateInfo()
{
   QDateTime lastUpdate = userSettings.value("LastAlert").toDateTime();
   return lastUpdate.toString("MMM dd yyyy, h:mm:ss ap");
}

QString MainWindow::getSigInfo()
{
    return getCmdOut("/usr/bin/gpgv --ignore-time-conflict --keyring /usr/share/mx-gpg-keys/mx-gpg-keyring "
                     "/var/tmp/mx-alerts/alert" + release + ".sig /var/tmp/mx-alerts/alert" + release +
                     " 2>&1 |grep 'Good signature from'").str;
}

void MainWindow::messageClicked()
{
    showLastAlert(true);
    //QTimer::singleShot(0, qApp, &QGuiApplication::quit);
}

// check updates and return true if one channel has a notification
bool MainWindow::checkUpdates()
{
    QString fileName = "alert" + release;
    QDateTime lastUpdate = userSettings.value("LastAlert").toDateTime();
    qDebug() << "Last update" <<  lastUpdate;
    if (!downloadFile(server + "/" + fileName + ".sig"))
        return false;
    qDebug() << "Header time" << reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();
    if (lastUpdate == reply->header(QNetworkRequest::LastModifiedHeader).toDateTime()) {
        qDebug() << "Already up-to-date";
        return false;
    }
    writeFile(".sig");
    userSettings.setValue("LastAlert", reply->header(QNetworkRequest::LastModifiedHeader).toDateTime());
    if (!downloadFile(server + "/" + fileName)) return false;
    writeFile();

    return showLastAlert();
}

bool MainWindow::verifySignature()
{
    return (getCmdOut("/usr/bin/gpgv --ignore-time-conflict --keyring /usr/share/mx-gpg-keys/mx-gpg-keyring  "
                      "/var/tmp/mx-alerts/alert" + release + ".sig /var/tmp/mx-alerts/alert" + release).exit_code == 0);
}

bool MainWindow::downloadFile(QUrl url)
{
    QNetworkReply *rep = manager.head(QNetworkRequest(url));
    QEventLoop loop;
    connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit); //manager.setTransferTimeout(time) only in Qt >= 5.15
    loop.exec();
    qlonglong size = rep->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    if (size > 4000) {
        qDebug() << QString("File sizs: %1 too big, won't download it").arg(size);
        return false;
    }

    reply = manager.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit); //manager.setTransferTimeout(time) only in Qt >= 5.15
    loop.exec();
    return (reply->error() == QNetworkReply::NoError);
}

void MainWindow::displayFile(QString fileName, bool clicked)
{
    bool first = true;
    QString title, body;

    QFile file(tmpFolder + fileName);
    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "Cannot open file";
        return;
    } else {
        QString line;
        while (!file.atEnd()) {
            line = file.readLine();
            if (line.startsWith("#") || line.isEmpty() || (first && line == "\n"))
                continue;
            if (first) {
                title = line;
                first = false;
            } else {
                body.append(line);
            }
        }
        if (!title.isEmpty()) {
            alertIcon->show();
            csleep(100);
            if (clicked) {
                setIcon("info");
                QMessageBox::critical(this, tr("MX Alerts") + ": " + title, title + "\n" + body + "\n" +
                                      "---------------------------------------------------------------------------------------\n" +
                                      tr("Release date/time: %1").arg(getDateInfo()) + "\n" +
                                      tr("Signature info: %1").arg(getSigInfo()));
            } else {
                showMessage(title, body);
            }
        }
    }
    file.close();
}

void MainWindow::createActions()
{
    aboutAction = new QAction(QIcon::fromTheme("help-about"), tr("&About"), this);
    hideAction = new QAction(QIcon::fromTheme("exit"), tr("&Hide until new alerts"), this);
    lastAlertAction = new QAction(QIcon::fromTheme("messagebox_critical"), tr("&Show last alert"), this);
    toggleDisableAction = new QAction(QIcon::fromTheme("notification-disabled-symbolic"), tr("&Disable autostart"), this);
    quitAction = new QAction(QIcon::fromTheme("gtk-quit"), tr("&Quit"), this);

    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    connect(toggleDisableAction, &QAction::triggered, this, &MainWindow::toggleDisabled);
    connect(hideAction, &QAction::triggered, qApp, &QGuiApplication::quit);
    connect(lastAlertAction, &QAction::triggered, this, [this](){showLastAlert(true);});
    connect(quitAction, &QAction::triggered, qApp, &QGuiApplication::quit);
}


// create menu and system tray icon
void MainWindow::createMenu()
{
    alertMenu = new QMenu(this);
    alertMenu->addAction(hideAction);
    alertMenu->addSeparator();
    alertMenu->addAction(lastAlertAction);
    alertMenu->addSeparator();
    alertMenu->addAction(toggleDisableAction);
    alertMenu->addSeparator();
    alertMenu->addAction(aboutAction);
    alertMenu->addSeparator();
    alertMenu->addAction(quitAction);

    alertIcon = new QSystemTrayIcon(this);
    alertIcon->setContextMenu(alertMenu);
}

void MainWindow::showAbout()
{
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About") + " MX Alerts", "<p align=\"center\"><b><h2>MX Alert</h2></b></p><p align=\"center\">" +
                       tr("Version: ") + VERSION + "</p><p align=\"center\"><h3>" +
                       tr("Displays alerts and urgent notifications from MX Linux team") +
                       "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p>"
                       "<p align=\"center\">" + tr("Copyright (c) MX Linux") + "<br /><br /></p>");
    QPushButton *btnLicense = msgBox.addButton(tr("License"), QMessageBox::HelpRole);
    QPushButton *btnChangelog = msgBox.addButton(tr("Changelog"), QMessageBox::HelpRole);
    QPushButton *btnCancel = msgBox.addButton(tr("Cancel"), QMessageBox::NoRole);
    btnCancel->setIcon(QIcon::fromTheme("window-close"));

    msgBox.exec();

    if (msgBox.clickedButton() == btnLicense) {
        system("xdg-open file:///usr/share/doc/mx-alerts/license.html");
    } else if (msgBox.clickedButton() == btnChangelog) {
        QDialog *changelog = new QDialog(this);
        changelog->resize(600, 500);

        QTextEdit *text = new QTextEdit;
        text->setReadOnly(true);
        text->setText(getCmdOut("zless /usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName()
                                + "/changelog.gz").str);

        QPushButton *btnClose = new QPushButton(tr("&Close"));
        btnClose->setIcon(QIcon::fromTheme("window-close"));
        connect(btnClose, &QPushButton::clicked, changelog, &QDialog::close);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(text);
        layout->addWidget(btnClose);
        changelog->setLayout(layout);
        changelog->exec();
    }
}

