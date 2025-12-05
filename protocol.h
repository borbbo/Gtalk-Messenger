#ifndef PROTOCOL_H
#define PROTOCOL_H

// Network Configuration
#define PORT 9999
#define BUF_SIZE 1024
#define MAX_CLNT 256

// Command Definitions
#define CMD_LOGIN 100   // Login request
#define CMD_MSG   200   // Normal message

// Data Packet Structure
typedef struct {
    int cmd;               // Command type (LOGIN, MSG, etc.)
    char id[20];           // User ID (Nickname)
    char msg[BUF_SIZE];    // Message content
} Packet;

#endif // PROTOCOL_H
