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
    this->setWindowTitle("Gtalk - Open Source Project");
}

MainWindow::~MainWindow()
{
    // Use ::close to avoid conflict with QMainWindow::close()
    if(sock != -1) ::close(sock);
    delete ui;
}

// [1] Connect Button Clicked
void MainWindow::on_connectBtn_clicked()
{
    // 1. Validate ID input
    myId = ui->idEdit->text();
    if(myId.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please enter your ID.");
        return;
    }

    struct sockaddr_in serv_addr;

    // 2. Create Socket
    sock = ::socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        QMessageBox::critical(this, "Error", "Socket creation failed!");
        return;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(PORT);

    // 3. Connect to Server
    if(::connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        QMessageBox::critical(this, "Error", "Connect failed! (Is server running?)");
        return;
    }

    // 4. Send Login Packet
    Packet p;
    p.cmd = CMD_LOGIN;
    strcpy(p.id, myId.toStdString().c_str());
    p.data_len = 0;
    ::write(sock, &p, sizeof(p));

    // 5. Update UI
    ui->textBrowser->append("=== Connected as " + myId + " ===");
    ui->connectBtn->setEnabled(false);
    ui->idEdit->setEnabled(false);

    // 6. Register Socket Notifier
    notifier = new QSocketNotifier(sock, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &MainWindow::on_socket_read);
}

// [2] Send Button Clicked
void MainWindow::on_sendBtn_clicked()
{
    if(sock == -1) return;

    QString msg = ui->lineEdit->text();
    if(msg.isEmpty()) return;

    Packet p;
    p.cmd = CMD_MSG;

    // Use stored 'myId'
    strcpy(p.id, myId.toStdString().c_str());

    // [Fixed] Use 'data' instead of 'msg'
    strcpy(p.data, msg.toStdString().c_str());
    p.data_len = strlen(p.data);

    ::write(sock, &p, sizeof(p));

    ui->lineEdit->clear();
}

// [3] Receive Data
void MainWindow::on_socket_read()
{
    Packet p;
    int str_len = ::read(sock, &p, sizeof(p));

    if(str_len == 0) {
        ui->textBrowser->append("=== Disconnected from Server ===");
        notifier->setEnabled(false);
        ::close(sock);
        sock = -1;
        ui->connectBtn->setEnabled(true);
        ui->idEdit->setEnabled(true);
        return;
    }

    // Handle Commands
    if (p.cmd == CMD_LOGIN) {
        ui->textBrowser->append(">>> " + QString(p.id) + " joined the chat.");
    }
    else if (p.cmd == CMD_MSG) {
        // [Fixed] Use 'data' instead of 'msg'
        QString showMsg = QString("[%1] %2").arg(p.id).arg(p.data);
        ui->textBrowser->append(showMsg);
    }
}
