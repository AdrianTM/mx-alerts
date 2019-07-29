#include <QtGui>
#include <QTextEdit>

#include "mainwindow.h"

MainWindow::MainWindow(const QStringList& args) :
    ui(new Ui::MainWindow),
    userSettings("MX-Linux", "mx-alerts")
{
    ui->setupUi(this);
    timer = new QTimer(this);
    manager = new QNetworkAccessManager(this);

    createActions();
    loadSettings();
    createMenu();

    connect(alertIcon, &QSystemTrayIcon::messageClicked, this, &MainWindow::messageClicked);
    connect(alertIcon, &QSystemTrayIcon::activated, this, &MainWindow::iconActivated);
    connect(timer, &QTimer::timeout, this, &MainWindow::checkUpdates);

    QDir dir;
    dir.mkdir(tmpFolder);

    setIcon("info");

    if (args.contains("--batch")) { // check for updates and exit when running --batch
        if (!checkUpdates()) {
            QTimer::singleShot(0, qApp, &QGuiApplication::quit);
        }
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
    QProcess *proc = new QProcess();
    QEventLoop loop;
    proc->setReadChannelMode(QProcess::MergedChannels);
    proc->start("/bin/bash", QStringList() << "-c" << cmd);
    proc->waitForFinished();
    qDebug() << "exitCode" << proc->exitCode();
    Output out = {proc->exitCode(), proc->readAll().trimmed()};
    delete proc;
    return out;
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

bool MainWindow::showLastAlert()
{
    if (!verifySignature()) {
        qDebug() << "Bad or missing signature";
        QFile::remove(tmpFolder + "alert");
        QFile::remove(tmpFolder + "alert.sig");
        userSettings.remove("Last_alerts");
        return false;
    } else {
        setIcon("messagebox_critical");
        displayFile("alert");
    }
    return true;
}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
        showLastAlert();
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
        ;
    }
}

void MainWindow::loadSettings()
{
    QSettings settings("/etc/mx-alerts.conf", QSettings::IniFormat);
    updateInterval = settings.value("UpdateInterval", "Hourly").toString().toLower();
    server = settings.value("Server").toString();

    // load user settings if present, otherwise use system settings
    updateInterval = userSettings.value("UpdateInterval", updateInterval).toString().toLower();
    server = userSettings.value("Server", server).toString();

    qDebug() << "Server" << server;
    qDebug() << "Update interval" << updateInterval;
    setTimeout();
}

void MainWindow::setDisabled(bool disabled)
{
    if (disabled) {
        // remove entries user crontab
        getCmdOut("crontab -l | sed '\\;/usr/bin/mx-alerts.sh;d'  | crontab -");
        getCmdOut("crontab -l | sed '\\;${HOME}/.mx-alerts/anacrontab;d'  | crontab -");
    } else {
        setSchedule(ui->combo_interval->currentText());
    }
}

void MainWindow::setTimeout()
{
    int timeout;
    ui->combo_interval->setCurrentIndex(ui->combo_interval->findText(updateInterval, Qt::MatchFixedString));
    if (updateInterval == "hourly") {
        timeout = HOUR_M;
    } else if (updateInterval == "daily") {
        timeout = DAY_M;
    } else if (updateInterval == "weekly") {
        timeout = WEEK_M;
    } else {
        return;
    }
    timer->start(timeout);
}

void MainWindow::showMessage(QString title, QString body, QString fileName)
{
    QSystemTrayIcon::MessageIcon icon = (fileName == "alert") ? QSystemTrayIcon::Critical : QSystemTrayIcon::Information;
    alertIcon->showMessage(title, body, icon, 0);
}

void MainWindow::setSchedule(QString newTiming)
{
    newTiming = newTiming.toLower();

    // remove entries user crontab
    getCmdOut("crontab -l | sed '\\;/usr/bin/mx-alerts.sh;d'  | crontab -");
    getCmdOut("crontab -l | sed '\\;${HOME}/.mx-alerts/anacrontab;d'  | crontab -");

    if (newTiming == "hourly") {
        getCmdOut("(crontab -l; echo '@hourly sleep $(( $(od -N1 -tuC -An /dev/urandom) \\% 30 ))m;/usr/bin/mx-alerts.sh')| crontab -");
    } else if (newTiming == "daily") {
        getCmdOut("mkdir -p ${HOME}/.mx-alerts/{etc,spool}");
        getCmdOut("echo '1 10 mx-alerts /usr/bin/mx-alerts.sh' > ${HOME}/.mx-alerts/anacrontab");
        getCmdOut("(crontab -l; echo '@hourly /usr/sbin/anacron -s -t ${HOME}/.mx-alerts/anacrontab -S ${HOME}/.mx-alerts/spool')| crontab -");
    } else { // weekly
        getCmdOut("mkdir -p ${HOME}/.mx-alerts/{etc,spool}");
        getCmdOut("echo '7 10 mx-alerts /usr/bin/mx-alerts.sh' > ${HOME}/.mx-alerts/anacrontab");
        getCmdOut("(crontab -l; echo '@hourly /usr/sbin/anacron -s -t ${HOME}/.mx-alerts/anacrontab -S ${HOME}/.mx-alerts/spool')| crontab -");
    }

    // Save timeout setting
    updateInterval = newTiming;
    userSettings.setValue("UpdateInterval", updateInterval);
    setTimeout();

}

void MainWindow::writeFile(QString extension)
{
    QFile file(tmpFolder + "alert" + extension);
    if (!file.open(QFile::WriteOnly)) {
        qDebug() << "Could not open file:" << file.fileName();
        QTimer::singleShot(0, qApp, &QGuiApplication::quit);
    }
    file.write(reply->readAll());
    file.close();
}

void MainWindow::messageClicked()
{
    QTimer::singleShot(0, qApp, &QGuiApplication::quit);
}

// check updates and return true if one channel has a notification
bool MainWindow::checkUpdates()
{
    QDateTime lastUpdate = userSettings.value("Last_alerts").toDateTime();
    qDebug() << "Last update" <<  lastUpdate;
    if (!downloadFile(server + "/" + "alert.sig")) return false;
    qDebug() << "Header time" << reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();
    if (lastUpdate == reply->header(QNetworkRequest::LastModifiedHeader).toDateTime()) {
        qDebug() << "Already up-to-date";
        return false;
    }
    writeFile(".sig");
    userSettings.setValue("Last_alerts", reply->header(QNetworkRequest::LastModifiedHeader).toDateTime());
    if (!downloadFile(server + "/alert")) return false;
    writeFile();

    return showLastAlert();
}

bool MainWindow::verifySignature()
{
    return (getCmdOut("gpg --verify /var/tmp/mx-alerts/alert.sig").exit_code == 0);
}

bool MainWindow::downloadFile(QUrl url)
{
    reply = manager->get(QNetworkRequest(url));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    return (reply->error() == QNetworkReply::NoError);
}

void MainWindow::displayFile(QString fileName)
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
            if (line.startsWith("#") || line.isEmpty() || (first && line == "\n")) {
                continue;
            }
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
            showMessage(title, body, fileName);
        }
    }
    file.close();
}

void MainWindow::createActions()
{
    aboutAction = new QAction(QIcon::fromTheme("help-about"), tr("&About"), this);
    hideAction = new QAction(QIcon::fromTheme("exit"), tr("&Hide until new alerts"), this);
    lastAlertAction = new QAction(QIcon::fromTheme("messagebox_critical"), tr("&Show last alert"), this);
    preferencesAction = new QAction(QIcon::fromTheme("settings"), tr("&Preferences"), this);
    quitAction = new QAction(QIcon::fromTheme("gtk-quit"), tr("&Quit"), this);

    connect(aboutAction, &QAction::triggered, this, &MainWindow::on_buttonAbout_clicked);
    connect(hideAction, &QAction::triggered, qApp, &QGuiApplication::quit);
    connect(lastAlertAction, &QAction::triggered, this, &MainWindow::showLastAlert);
    connect(preferencesAction, &QAction::triggered, this, &MainWindow::showNormal);
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
    alertMenu->addAction(preferencesAction);
    alertMenu->addSeparator();
    alertMenu->addAction(aboutAction);
    alertMenu->addSeparator();
    alertMenu->addAction(quitAction);

    alertIcon = new QSystemTrayIcon(this);
    alertIcon->setContextMenu(alertMenu);
}

void MainWindow::on_buttonAbout_clicked()
{
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About") + " MX Alerts", "<p align=\"center\"><b><h2>MX Alert</h2></b></p><p align=\"center\">" +
                       tr("Version: ") + VERSION + "</p><p align=\"center\"><h3>" +
                       tr("Displays alerts and urgent notifications from MX Linux team") +
                       "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) MX Linux") + "<br /><br /></p>");
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
        text->setText(getCmdOut("zless /usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName()  + "/changelog.gz").str);

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

void MainWindow::on_buttonApply_clicked()
{
    setSchedule(ui->combo_interval->currentText());
    setTimeout();
    setDisabled(ui->cb_disable->isChecked());
    close();
    QTimer::singleShot(0, qApp, &QGuiApplication::quit);
}

