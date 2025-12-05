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


/* ============================================================
 * [1] CONNECT BUTTON CLICKED
 * ============================================================ */
void MainWindow::on_connectBtn_clicked()
{
    myId = ui->idEdit->text();
    if(myId.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please enter your ID.");
        return;
    }

    struct sockaddr_in serv_addr;

    sock = ::socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        QMessageBox::critical(this, "Error", "Socket creation failed!");
        return;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(PORT);

    if(::connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        QMessageBox::critical(this, "Error", "Connect failed! (Is server running?)");
        return;
    }

    Packet p;
    p.cmd = CMD_LOGIN;
    strcpy(p.id, myId.toStdString().c_str());
    p.data_len = 0;
    ::write(sock, &p, sizeof(p));

    ui->textBrowser->append("=== Connected as " + myId + " ===");
    ui->connectBtn->setEnabled(false);
    ui->idEdit->setEnabled(false);

    notifier = new QSocketNotifier(sock, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &MainWindow::on_socket_read);
}

/* ============================================================
 * [2] PART B — SEND MESSAGE
 * ============================================================ */
void MainWindow::on_sendBtn_clicked()
{
    if(sock == -1) return;

    QString msg = ui->msgEdit->text();
    if(msg.isEmpty()) return;

    Packet p;
    p.cmd = CMD_MSG;
    strcpy(p.id, myId.toStdString().c_str());
    strcpy(p.msg, msg.toStdString().c_str());
    p.roomID = currentRoomID;

    ::write(sock, &p, sizeof(p));

    ui->msgEdit->clear();
}

/* ============================================================
 * [3] PART B — FILE TRANSFER
 * ============================================================ */
void MainWindow::on_fileBtn_clicked()
{
    if(sock == -1) {
        QMessageBox::warning(this, "Warning", "Please connect to the server first.");
        return;
    }

    QString fileName = QFileDialog::getOpenFileName(this, "Select File");
    if(fileName.isEmpty()) return;

    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Cannot open file!");
        return;
    }

    QByteArray fileData = file.read(BUF_SIZE);
    QFileInfo fileInfo(fileName);

    Packet p;
    p.cmd = CMD_FILE;
    strcpy(p.id, myId.toStdString().c_str());
    strcpy(p.fileName, fileInfo.fileName().toStdString().c_str());
    memcpy(p.data, fileData.data(), fileData.size());
    p.data_len = fileData.size();
    p.roomID = currentRoomID;

    ::write(sock, &p, sizeof(p));
    ui->textBrowser->append("[System] File sent: " + fileInfo.fileName());

    file.close();
}

/* ============================================================
 * [4] PART B — LEAVE ROOM
 * ============================================================ */
void MainWindow::on_leaveBtn_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
    ui->textBrowser->clear();
    currentRoomID = -1;

    on_refreshBtn_clicked();
}

/* ============================================================
 * [5] RECEIVE PACKETS FROM SERVER
 * ============================================================ */
void MainWindow::on_socket_read()
{
    Packet p;
    int str_len = ::read(sock, &p, sizeof(p));

    if(str_len == 0) {
        ui->textBrowser->append("=== Disconnected ===");
        notifier->setEnabled(false);
        ::close(sock);
        sock = -1;
        ui->connectBtn->setEnabled(true);
        ui->idEdit->setEnabled(true);
        return;
    }

    if(p.cmd == CMD_LOGIN) {
        ui->textBrowser->append(">>> " + QString(p.id) + " joined the chat.");
    }
    else if(p.cmd == CMD_MSG) {
        QString showMsg = QString("[%1] %2").arg(p.id).arg(p.msg);
        ui->textBrowser->append(showMsg);
    }
    else if(p.cmd == CMD_FILE) {
        QString sender = QString(p.id);
        QString fName = QString(p.fileName);
        ui->textBrowser->append(">>> Incoming File from " + sender + ": " + fName);
    }
}
