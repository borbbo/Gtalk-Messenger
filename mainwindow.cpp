#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>

// Linux System Headers
extern "C" {
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <string.h>
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    sock = -1;
}

MainWindow::~MainWindow()
{
    // Use ::close to avoid conflict with QMainWindow::close()
    if(sock != -1) ::close(sock);
    delete ui;
}

// [1] Connect Button
void MainWindow::on_connectBtn_clicked()
{
    struct sockaddr_in serv_addr;

    // 1. Create Socket
    sock = ::socket(PF_INET, SOCK_STREAM, 0); // Added ::
    if(sock == -1) {
        QMessageBox::critical(this, "Error", "Socket creation failed!");
        return;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(PORT);

    // 2. Connect (Added :: to verify it is Linux connect, not Qt connect)
    if(::connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        QMessageBox::critical(this, "Error", "Connect failed! (Is server running?)");
        return;
    }

    // 3. UI Update
    ui->textBrowser->append("=== Connected to Server ===");
    ui->connectBtn->setEnabled(false);

    // 4. Register Socket Notifier
    notifier = new QSocketNotifier(sock, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &MainWindow::on_socket_read);
}

// [2] Send Button
void MainWindow::on_sendBtn_clicked()
{
    if(sock == -1) return;

    QString msg = ui->lineEdit->text();
    if(msg.isEmpty()) return;

    Packet p;
    p.cmd = CMD_MSG;
    strcpy(p.id, "Bomin");
    strcpy(p.msg, msg.toStdString().c_str());

    // Send data (Added ::)
    ::write(sock, &p, sizeof(p));

    ui->lineEdit->clear();
}

// [3] Receive Data
void MainWindow::on_socket_read()
{
    Packet p;
    // Read data (Added ::)
    int str_len = ::read(sock, &p, sizeof(p));

    if(str_len == 0) {
        ui->textBrowser->append("=== Disconnected ===");
        notifier->setEnabled(false);
        ::close(sock); // Added ::
        sock = -1;
        ui->connectBtn->setEnabled(true);
        return;
    }

    QString showMsg = QString("[%1] %2").arg(p.id).arg(p.msg);
    ui->textBrowser->append(showMsg);
}
