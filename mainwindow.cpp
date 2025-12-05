#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QInputDialog> // Required for Room Title Input

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
    currentRoomID = -1;
    this->setWindowTitle("Gtalk - Advanced Messenger");

    // Start at Page 0 (Login Screen)
    ui->stackedWidget->setCurrentIndex(0);
}

MainWindow::~MainWindow()
{
    if(sock != -1) ::close(sock);
    delete ui;
}

// === [Helper: Server Connection Logic] (Implemented by Team Leader) ===
void MainWindow::connectToServer() {
    if(sock != -1) return; // Already connected

    sock = ::socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        QMessageBox::critical(this, "Error", "Socket creation failed!");
        return;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Localhost
    serv_addr.sin_port = htons(PORT);

    if(::connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        QMessageBox::critical(this, "Error", "Cannot connect to server! Is it running?");
        sock = -1;
        return;
    }

    notifier = new QSocketNotifier(sock, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &MainWindow::on_socket_read);
}

// ============================================================
// [PART A] User Authentication & Lobby Logic
// To be implemented by Team Member A
// ============================================================

void MainWindow::on_loginBtn_clicked() {
    // TODO: Member A - Implement Login Logic
}

void MainWindow::on_registerBtn_clicked() {
    // TODO: Member A - Implement Register Logic
}

void MainWindow::on_createRoomBtn_clicked() {
    // TODO: Member A - Implement Create Room Logic
}

void MainWindow::on_refreshBtn_clicked() {
    // TODO: Member A - Implement Refresh Room List Logic
}

void MainWindow::on_roomListWidget_itemDoubleClicked(QListWidgetItem *item) {
    // TODO: Member A - Implement Join Room Logic
    (void)item; // Unused parameter warning suppression
}

// ============================================================
// [PART B] Chat & File Transfer Logic
// To be implemented by Team Member B
// ============================================================

void MainWindow::on_sendBtn_clicked() {
    // TODO: Member B - Implement Send Message Logic
}

void MainWindow::on_fileBtn_clicked() {
    // TODO: Member B - Implement File Transfer Logic
}

void MainWindow::on_leaveBtn_clicked() {
    // TODO: Member B - Implement Leave Room Logic
}

// ============================================================
// [Network Handler] (Managed by Team Leader)
// ============================================================
void MainWindow::on_socket_read() {
    Packet p;
    int len = ::read(sock, &p, sizeof(p));
    if(len <= 0) {
        ui->stackedWidget->setCurrentIndex(0);
        ::close(sock);
        sock = -1;
        return;
    }

    // Packet Handling Logic (Partially implemented)
    // Team Leader will finalize this part after merging A and B's work.

    if(p.cmd == CMD_LOGIN_OK) {
        QMessageBox::information(this, "Success", p.msg);
        ui->stackedWidget->setCurrentIndex(1); // Go to Lobby
        on_refreshBtn_clicked();
    }
    else if(p.cmd == CMD_LOGIN_FAIL) {
        QMessageBox::warning(this, "Login Failed", p.msg);
    }
    else if(p.cmd == CMD_ROOM_LIST) {
        ui->roomListWidget->clear();
        QString listStr = QString::fromUtf8(p.msg);
        QStringList rooms = listStr.split(",", Qt::SkipEmptyParts);
        for(const QString& r : rooms) ui->roomListWidget->addItem(r);
    }
    else if(p.cmd == CMD_JOIN_ROOM) {
        currentRoomID = p.roomID;
        ui->stackedWidget->setCurrentIndex(2); // Go to Chat Room
        ui->textBrowser->append("=== Entered Room: " + QString::fromUtf8(p.msg) + " ===");
    }
    else if(p.cmd == CMD_MSG) {
        ui->textBrowser->append("[" + QString(p.id) + "] " + QString::fromUtf8(p.msg));
    }
    else if(p.cmd == CMD_FILE) {
        ui->textBrowser->append(">>> File from " + QString(p.id) + ": " + QString(p.fileName));
    }
}
