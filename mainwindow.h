#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <version.h>

// struct for outputing both the exit code and the strings when running a command
struct Output {
    int exit_code;
    QString str;
};

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    MainWindow(const QStringList &args);
    Output getCmdOut(const QString &cmd);

private slots:
    bool checkUpdates();
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void messageClicked();
    void setIcon(QString icon_name);
    void showAbout();

private:
    bool downloadFile(QUrl url);
    bool showLastAlert();
    bool verifySignature();

    void displayFile(QString fileName);
    void createActions();
    void createMenu();
    void csleep(int msec);
    void loadSettings();
    void setDisabled();
    void showMessage(QString title, QString body, QString fileName);
    void writeFile(QString extension = "");

    QString updateInterval;
    QString server;
    QString tmpFolder = "/var/tmp/mx-alerts/";
    QSettings userSettings;

    QAction *aboutAction;
    QAction *disableAction;
    QAction *hideAction;
    QAction *lastAlertAction;
    QAction *lastNewsAction;
    QAction *quitAction;

    QSystemTrayIcon *alertIcon;
    QMenu *alertMenu;

    QNetworkAccessManager *manager;
    QNetworkReply *reply;

    QTimer *timer;
};

#endif

