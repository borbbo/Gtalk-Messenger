#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PORT 9999
#define BUF_SIZE 4096  // Buffer size
#define MAX_CLNT 256

// --- Command Definitions ---

// [User Management]
#define CMD_REGISTER    100 // Register new user
#define CMD_LOGIN       101 // Login request
#define CMD_LOGIN_OK    102 // Login success
#define CMD_LOGIN_FAIL  103 // Login failed

// [Room Management]
#define CMD_CREATE_ROOM 200 // Create a new chat room
#define CMD_JOIN_ROOM   201 // Join an existing room
#define CMD_LEAVE_ROOM  202 // Leave room (Go to Lobby)
#define CMD_ROOM_LIST   203 // Request room list
#define CMD_USER_LIST   204 // userlist

// [Chat & File]
#define CMD_MSG         300 // Send Message
#define CMD_FILE        301 // [FIXED] Changed to 301 to avoid conflict with CMD_MSG
#define CMD_FILE_REQ    400 // File Transfer Request (Optional)
#define CMD_FILE_ACK    401 // File Transfer Accepted (Optional)

// --- Packet Structure ---
typedef struct {
    int cmd;            // Command type
    char id[20];        // User ID
    char pwd[20];       // Password
    char msg[BUF_SIZE]; // Message OR Room Title OR Room List
    int roomID;         // Room ID (-1: Lobby)
    int data_len;       // Data size
    char fileName[100]; // File Name
    char data[BUF_SIZE];// Binary Data Payload
} Packet;

#endif // PROTOCOL_H
