#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSocketNotifier> // Class for monitoring file descriptors (sockets)
#include "protocol.h"      // Protocol definition header

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
    // Slot functions for UI button clicks
    void on_connectBtn_clicked();
    void on_sendBtn_clicked();

    // Slot function to handle incoming data from the server
    void on_socket_read();

private:
    Ui::MainWindow *ui;
    int sock;                   // Linux socket file descriptor
    QSocketNotifier *notifier;  // Socket event notifier

    QString myId; // variable: login id
};
#endif // MAINWINDOW_H
