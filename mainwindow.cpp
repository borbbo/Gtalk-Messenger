#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QDir>

// Headers for Custom Dialog
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QDialogButtonBox>

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

    // enter -> send
    connect(ui->msgEdit, &QLineEdit::returnPressed, this, &MainWindow::on_sendBtn_clicked);
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
// To be implemented by Team Member siyeon
// ============================================================

// 1. Login Button Logic
void MainWindow::on_loginBtn_clicked() {
    QString id = ui->loginIdEdit->text();
    QString pw = ui->loginPwEdit->text();

    if(id.isEmpty() || pw.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please enter ID and Password.");
        return;
    }

    connectToServer(); // Connect to server
    if(sock == -1) return;

    Packet p;
    p.cmd = CMD_LOGIN;
    strcpy(p.id, id.toStdString().c_str());
    strcpy(p.pwd, pw.toStdString().c_str());
    ::write(sock, &p, sizeof(p));

    myId = id; // Store ID temporarily
}

// 2. Register Button Logic
void MainWindow::on_registerBtn_clicked() {
    QString id = ui->loginIdEdit->text();
    QString pw = ui->loginPwEdit->text();

    if(id.isEmpty() || pw.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter ID/PW for registration.");
        return;
    }

    connectToServer();
    if(sock == -1) return;

    Packet p;
    p.cmd = CMD_REGISTER;
    strcpy(p.id, id.toStdString().c_str());
    strcpy(p.pwd, pw.toStdString().c_str());
    ::write(sock, &p, sizeof(p));
}

// 3. Create Room Button Logic (Updated: Custom Dialog)
void MainWindow::on_createRoomBtn_clicked() {
    // Create a custom dialog
    QDialog dialog(this);
    dialog.setWindowTitle("Create Room");

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    layout->setSpacing(5);

    // Room Title Input
    QLabel *titleLabel = new QLabel("Room Title:");
    QLineEdit *titleEdit = new QLineEdit();
    layout->addWidget(titleLabel);
    layout->addWidget(titleEdit);

    // Nickname Input
    QLabel *nickLabel = new QLabel("Your Nickname:");
    QLineEdit *nickEdit = new QLineEdit();
    nickEdit->setText(myId); // default nickname is ID
    layout->addWidget(nickLabel);
    layout->addWidget(nickEdit);

    // Password Checkbox
    QCheckBox *pwCheck = new QCheckBox("Use Password");
    layout->addWidget(pwCheck);

    // Password Input (Initially disabled)
    QLineEdit *pwEdit = new QLineEdit();
    pwEdit->setPlaceholderText("Password");
    pwEdit->setEchoMode(QLineEdit::Password);
    pwEdit->setEnabled(false); // Default: disabled
    layout->addWidget(pwEdit);

    // Enable password input only when checked
    connect(pwCheck, &QCheckBox::toggled, pwEdit, &QLineEdit::setEnabled);

    layout->addSpacing(15);

    // Buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    // Show dialog
    if (dialog.exec() == QDialog::Accepted) {
        QString title = titleEdit->text();
        QString nickname = nickEdit->text();
        QString pwd = pwCheck->isChecked() ? pwEdit->text() : "";

        if(title.isEmpty()) return;
        if(nickname.isEmpty()) nickname = myId;

        Packet p;
        p.cmd = CMD_CREATE_ROOM;
        strcpy(p.id, myId.toStdString().c_str());
        strcpy(p.msg, title.toStdString().c_str());
        strcpy(p.pwd, pwd.toStdString().c_str());

        // send nickname via fileName field
        strcpy(p.fileName, nickname.toStdString().c_str());

        ::write(sock, &p, sizeof(p));
    }
}

// 4. Refresh Button Logic
void MainWindow::on_refreshBtn_clicked() {
    Packet p;
    p.cmd = CMD_ROOM_LIST;
    strcpy(p.id, myId.toStdString().c_str());
    ::write(sock, &p, sizeof(p));
}

// 5. Join Room Logic (Double Click)
void MainWindow::on_roomListWidget_itemDoubleClicked(QListWidgetItem *item) {
    QString text = item->text();
    // Parse Room ID from string "1 room: Title (Private)"
    QStringList parts = text.split(" ");
    if(parts.isEmpty()) return;

    int roomID = parts.first().toInt();

    // ask for nickname first
    bool ok;
    QString nickname = QInputDialog::getText(this, "Set Nickname", "Enter your nickname for this room:", QLineEdit::Normal, myId, &ok);
    if (!ok) return;
    if (nickname.isEmpty()) nickname = myId;

    // pwd check (Updated: check for English text)
    QString pwd = "";
    if (text.contains("(Private)")) {
        pwd = QInputDialog::getText(this, "Private Room", "Enter Password:", QLineEdit::Password, "", &ok);
        if (!ok) return;
    }

    Packet p;
    p.cmd = CMD_JOIN_ROOM;
    strcpy(p.id, myId.toStdString().c_str());
    p.roomID = roomID;
    strcpy(p.pwd, pwd.toStdString().c_str());

    // send nickname via msg field
    strcpy(p.msg, nickname.toStdString().c_str());

    ::write(sock, &p, sizeof(p));
}

// ============================================================
// [PART B] Chat & File Transfer Logic
// To be implemented by Team Member nyeongyeong
// ============================================================

// 1. Send Message Logic
void MainWindow::on_sendBtn_clicked() {
    QString msg = ui->msgEdit->text();
    if(msg.isEmpty()) return;

    Packet p;
    p.cmd = CMD_MSG;
    strcpy(p.id, myId.toStdString().c_str());
    strcpy(p.msg, msg.toStdString().c_str());
    p.roomID = currentRoomID; // Send to current room only
    ::write(sock, &p, sizeof(p));

    ui->msgEdit->clear();
}

// 2. File Transfer Logic
void MainWindow::on_fileBtn_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "Select File");
    if(fileName.isEmpty()) return;

    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) return;

    // Read file data
    QByteArray fileData = file.read(BUF_SIZE);
    QFileInfo fileInfo(fileName);

    Packet p;
    p.cmd = CMD_FILE; // Protocol 301
    strcpy(p.id, myId.toStdString().c_str());
    strcpy(p.fileName, fileInfo.fileName().toStdString().c_str());
    memcpy(p.data, fileData.data(), fileData.size()); // Binary copy
    p.data_len = fileData.size();
    p.roomID = currentRoomID;

    ::write(sock, &p, sizeof(p));

    ui->textBrowser->append("[System] File sent: " + fileInfo.fileName());
    file.close();
}

// 3. Leave Room Logic
void MainWindow::on_leaveBtn_clicked() {
    // Send LEAVE packet to server
    Packet p;
    p.cmd = CMD_LEAVE_ROOM;
    strcpy(p.id, myId.toStdString().c_str());
    ::write(sock, &p, sizeof(p));

    // Go back to Lobby (Page 1)
    ui->stackedWidget->setCurrentIndex(1);

    // Clear UI
    ui->textBrowser->clear();
    ui->userListWidget->clear();
    currentRoomID = -1;

    // Refresh room list
    on_refreshBtn_clicked();
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
        QMessageBox::warning(this, "Error", p.msg);
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
        ui->msgEdit->setFocus();
    }
    else if(p.cmd == CMD_MSG) {
        if(strcmp(p.id, "System") == 0) {
             ui->textBrowser->append("<font color='blue'><b>[System] " + QString::fromUtf8(p.msg) + "</b></font>");
        } else {
             ui->textBrowser->append("[" + QString(p.id) + "] " + QString::fromUtf8(p.msg));
        }
    }
    else if(p.cmd == CMD_FILE) {
            QString sender = QString(p.id);         // Sender's nickname
            QString fileName = QString(p.fileName); // File name

            // Save received data to file. Save to Home Directory
            QString saveName = QDir::homePath() + "/received_" + fileName;

            QFile file(saveName);
            if(file.open(QIODevice::WriteOnly)) {
                file.write(p.data, p.data_len); // Write binary data from packet
                file.close();

                ui->textBrowser->append(">>> [File Saved] " + saveName + " (from " + sender + ")");
            } else {
                ui->textBrowser->append(">>> [Error] Failed to save file.");
            }
        }
    else if(p.cmd == CMD_USER_LIST) {
        ui->userListWidget->clear();
        QString listStr = QString::fromUtf8(p.msg);
        QStringList users = listStr.split(",", Qt::SkipEmptyParts);
        for(const QString& u : users) ui->userListWidget->addItem(u);
    }
}
