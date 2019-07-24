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
#include "ui_mainwindow.h"

namespace Ui {
class MainWindow;
}

// struct for outputing both the exit code and the strings when running a command
struct Output {
    int exit_code;
    QString str;
};

static int HOUR_M = 60 * 60 * 1000;
static int DAY_M = HOUR_M * 24;
static int WEEK_M = DAY_M * 7;

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
    void showLastAlert();
    void showLastNews();
    void on_buttonAbout_clicked();
    void on_buttonApply_clicked();

private:
    Ui::MainWindow *ui;

    bool checkChannel(QString channel);
    bool downloadFile(QUrl url);
    bool showChannel(QString channel);
    bool verifySignature(QString channel);

    void displayFile(QString fileName);
    void createActions();
    void createMenu();
    void createComboChannel();
    void csleep(int msec);
    void loadSettings();
    void setChannel();
    void setDisabled(bool disabled);
    void setSchedule(QString newTiming);
    void setTimeout();
    void showMessage(QString title, QString body, QString fileName);
    void writeFile(QString channel, QString extension = "");

    QString notificationLevel;
    QString updateInterval;
    QString server;
    QString tmpFolder = "/var/tmp/mx-alerts/";
    QSettings userSettings;

    QAction *aboutAction;
    QAction *hideAction;
    QAction *lastAlertAction;
    QAction *lastNewsAction;
    QAction *preferencesAction;
    QAction *quitAction;

    QSystemTrayIcon *alertIcon;
    QMenu *alertMenu;

    QNetworkAccessManager *manager;
    QNetworkReply *reply;

    QTimer *timer;
};

#endif

