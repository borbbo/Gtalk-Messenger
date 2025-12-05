#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSocketNotifier>
#include <QFileDialog>
#include <QListWidgetItem> // Required for handling room list clicks
#include "protocol.h"      // Must use the updated V2 protocol

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // [Page 0: Login Screen]
    void on_loginBtn_clicked();
    void on_registerBtn_clicked();

    // [Page 1: Lobby Screen]
    void on_createRoomBtn_clicked();
    void on_refreshBtn_clicked();
    void on_roomListWidget_itemDoubleClicked(QListWidgetItem *item); // Join room on double click

    // [Page 2: Chat Room Screen]
    void on_sendBtn_clicked();
    void on_fileBtn_clicked();
    void on_leaveBtn_clicked(); // Exit room and return to lobby

    // [Network Handler]
    void on_socket_read();

private:
    Ui::MainWindow *ui;
    int sock;
    QSocketNotifier *notifier;

    // User Session Data
    QString myId;
    int currentRoomID; // -1: Lobby, >=0: In a Room

    // Helper function to establish connection
    void connectToServer();
};
#endif // MAINWINDOW_H
