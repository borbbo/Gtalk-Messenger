#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PORT 9999
#define BUF_SIZE 4096
#define MAX_CLNT 256

// Command Definitions
#define CMD_LOGIN 100  // Login
#define CMD_MSG   200  // Chat Message
#define CMD_FILE  300  // File Transfer

typedef struct {
    int cmd;            // Command Type (LOGIN, MSG, FILE)
    char id[20];        // Sender ID
    int data_len;       // Data size (Required for file transfer)
    char fileName[100]; // Filename (Used only for CMD_FILE)
    char data[BUF_SIZE];// Payload (Chat message OR File binary data)
} Packet;

#endif // PROTOCOL_H
