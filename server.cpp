#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>

// Linux Network Headers
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <string.h>
#include <fcntl.h>

#include "protocol.h"

using namespace std;

#define EPOLL_SIZE 500
const string USER_FILE = "users.txt"; // File to store user credentials

// Data Structure: User
struct User {
    int socket;
    string id;
    string nickname; // nickname for chat room
    int currentRoomID; // -1: Lobby, >=0: Specific Room ID
    bool isLoggedIn;
};

// Data Structure: Room
struct Room {
    int id;
    string title;
    string password;
    vector<int> userSockets; // List of sockets in this room
};

// Global Variables
map<int, User*> connectedUsers;   // Key: Socket, Value: User Info
map<int, Room*> activeRooms;      // Key: RoomID, Value: Room Info
map<string, string> userDB;       // Key: ID, Value: Password (Loaded from file)
int roomIDCounter = 1;            // Auto-increment for Room IDs

// Function Declarations
void handle_packet(int sock, Packet& p);
void send_packet(int sock, Packet& p);
void load_users();
void save_user(string id, string pw);
void error_handling(string msg);
void remove_user(int sock);
void broadcast_room_users(int roomID);
void leave_room_logic(int sock, int roomID); // helper for leave logic

int main() {
    // 1. Load User DB from file
    load_users();

    // 2. Server Socket Setup
    int serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_adr;
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(PORT);

    // Reuse Port option (prevents bind error)
    int opt = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // 3. Epoll Setup
    int epoll_fd = epoll_create(EPOLL_SIZE);
    struct epoll_event *ep_events = new epoll_event[EPOLL_SIZE];
    struct epoll_event event;

    event.events = EPOLLIN;
    event.data.fd = serv_sock;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serv_sock, &event);

    cout << ">>> Gtalk Advanced Server Started on Port " << PORT << endl;

    while (true) {
        int event_cnt = epoll_wait(epoll_fd, ep_events, EPOLL_SIZE, -1);
        if (event_cnt == -1) break;

        for (int i = 0; i < event_cnt; i++) {
            int cur_fd = ep_events[i].data.fd;

            if (cur_fd == serv_sock) {
                // New Connection
                struct sockaddr_in clnt_adr;
                socklen_t adr_sz = sizeof(clnt_adr);
                int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);

                event.events = EPOLLIN;
                event.data.fd = clnt_sock;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clnt_sock, &event);

                // Create temporary User object (Not logged in yet)
                User* newUser = new User{clnt_sock, "", "", -1, false};
                connectedUsers[clnt_sock] = newUser;
                cout << "[System] Client connected: " << clnt_sock << endl;
            }
            else {
                // Data Received
                Packet p;
                int str_len = read(cur_fd, &p, sizeof(Packet));

                if (str_len <= 0) {
                    // Disconnection
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cur_fd, NULL);
                    close(cur_fd);
                    remove_user(cur_fd); // Clean up user data
                    cout << "[System] Client disconnected: " << cur_fd << endl;
                }
                else {
                    // Process Packet
                    handle_packet(cur_fd, p);
                }
            }
        }
    }
    close(serv_sock);
    delete[] ep_events;
    return 0;
}

// === Logic for Handling Packets ===
void handle_packet(int sock, Packet& p) {
    User* u = connectedUsers[sock];
    Packet res; // Response Packet
    memset(&res, 0, sizeof(Packet));

    switch (p.cmd) {
        // [1] Register (Sign Up)
        case CMD_REGISTER: {
            if (userDB.find(p.id) != userDB.end()) {
                res.cmd = CMD_LOGIN_FAIL;
                strcpy(res.msg, "ID already exists.");
            } else {
                save_user(p.id, p.pwd); // Save to file
                userDB[p.id] = p.pwd;   // Update memory
                res.cmd = CMD_LOGIN_OK;
                strcpy(res.msg, "Registration Success! Please Login.");
                cout << "[Register] New User: " << p.id << endl;
            }
            send_packet(sock, res);
            break;
        }

        // [2] Login
        case CMD_LOGIN: {
            // Check ID and Password
            if (userDB.find(p.id) != userDB.end() && userDB[p.id] == p.pwd) {
                u->id = p.id;
                u->isLoggedIn = true;
                u->currentRoomID = -1; // Default: Lobby

                res.cmd = CMD_LOGIN_OK;
                strcpy(res.msg, "Login Success!");
                cout << "[Login] User: " << u->id << endl;
            } else {
                res.cmd = CMD_LOGIN_FAIL;
                strcpy(res.msg, "Invalid ID or Password.");
            }
            send_packet(sock, res);
            break;
        }

        // [3] Create Room
        case CMD_CREATE_ROOM: {
            if (!u->isLoggedIn) return;

            Room* newRoom = new Room();
            newRoom->id = roomIDCounter++;
            newRoom->title = p.msg; // Room Title comes in 'msg' field
            newRoom->password = p.pwd;
            newRoom->userSockets.push_back(sock);

            // store nickname from fileName field
            u->nickname = string(p.fileName);
            if(u->nickname.empty()) u->nickname = u->id;

            activeRooms[newRoom->id] = newRoom;
            u->currentRoomID = newRoom->id;

            // Notify User
            res.cmd = CMD_JOIN_ROOM;
            res.roomID = newRoom->id;
            strcpy(res.msg, newRoom->title.c_str());
            send_packet(sock, res);

            cout << "[Room] Created: " << newRoom->title << " by " << u->nickname << endl;
            broadcast_room_users(newRoom->id);
            break;
        }

        // [4] Join Room
        case CMD_JOIN_ROOM: {
             if (!u->isLoggedIn) return;
             int targetRoomID = p.roomID;

             if (activeRooms.find(targetRoomID) != activeRooms.end()) {
                 Room* r = activeRooms[targetRoomID];

                 // Check Password
                 if (!r->password.empty() && r->password != string(p.pwd)) {
                     // Wrong password -> Send failure packet
                     Packet errP;
                     errP.cmd = CMD_LOGIN_FAIL;
                     strcpy(errP.msg, "Incorrect Password!");
                     send_packet(sock, errP);
                     return;
                 }

                 // check duplicate socket to avoid triple message
                 bool alreadyIn = false;
                 for(int s : r->userSockets) {
                     if(s == sock) { alreadyIn = true; break; }
                 }
                 if(!alreadyIn) {
                     r->userSockets.push_back(sock);
                 }

                 // store nickname from msg field
                 u->nickname = string(p.msg);
                 if(u->nickname.empty()) u->nickname = u->id;

                 u->currentRoomID = targetRoomID;

                 res.cmd = CMD_JOIN_ROOM;
                 res.roomID = r->id;
                 strcpy(res.msg, r->title.c_str());
                 send_packet(sock, res);

                 Packet joinMsg;
                 joinMsg.cmd = CMD_MSG;
                 joinMsg.roomID = r->id;
                 strcpy(joinMsg.id, "System");

                 // use nickname for system message
                 string noti = u->nickname + " joined the chat.";
                 strcpy(joinMsg.msg, noti.c_str());

                 for (int s : r->userSockets) {
                     if (s != sock) send_packet(s, joinMsg);
                 }

                 // refresh userlist
                 broadcast_room_users(r->id);
             }
             break;
        }

        // Leave Room
        case CMD_LEAVE_ROOM: {
            if (!u->isLoggedIn || u->currentRoomID == -1) return;
            leave_room_logic(sock, u->currentRoomID);
            u->currentRoomID = -1; // back to lobby
            break;
        }

        // [5] Room List Request (For Lobby)
        case CMD_ROOM_LIST: {
             string listStr = "";
             for(auto const& [key, val] : activeRooms) {
                 // Changed lock emoji to text "(Private)"
                 string lockIcon = val->password.empty() ? "" : " (Private)";
                 // Format: "ID room: Title"
                 listStr += to_string(key) + " room: " + val->title + lockIcon + ",";
             }
             res.cmd = CMD_ROOM_LIST;
             strcpy(res.msg, listStr.c_str());
             send_packet(sock, res);
             break;
        }

        // [6] Chat Message
        case CMD_MSG: {
            if (!u->isLoggedIn || u->currentRoomID == -1) return; // Chat only allowed in Rooms

            Room* r = activeRooms[u->currentRoomID];
            if (r) {
                // replace ID with nickname
                strcpy(p.id, u->nickname.c_str());

                // Broadcast to everyone in the room
                for (int s : r->userSockets) {
                    send_packet(s, p);
                }
            }
            break;
        }

        // [7] File Transfer (Relay to Room)
        case CMD_FILE: {
             if (!u->isLoggedIn || u->currentRoomID == -1) return;
             Room* r = activeRooms[u->currentRoomID];
             if (r) {
                 // replace ID with nickname
                 strcpy(p.id, u->nickname.c_str());

                 for (int s : r->userSockets) {
                     if (s != sock) send_packet(s, p); // Don't echo back to sender
                 }
                 cout << "[File] " << u->nickname << " sent a file." << endl;
             }
             break;
        }
    }
}

void send_packet(int sock, Packet& p) {
    write(sock, &p, sizeof(Packet));
}

void load_users() {
    ifstream file(USER_FILE);
    if (!file.is_open()) return;
    string id, pw;
    while (file >> id >> pw) {
        userDB[id] = pw;
    }
    file.close();
    cout << "[System] User DB Loaded: " << userDB.size() << " users." << endl;
}

void save_user(string id, string pw) {
    ofstream file(USER_FILE, ios::app);
    file << id << " " << pw << endl;
    file.close();
}

// Logic for leaving room (used by CMD_LEAVE_ROOM and remove_user)
void leave_room_logic(int sock, int roomID) {
    if (activeRooms.find(roomID) == activeRooms.end()) return;
    Room* r = activeRooms[roomID];
    User* u = connectedUsers[sock];

    // remove socket from vector
    for(auto it = r->userSockets.begin(); it != r->userSockets.end(); ) {
        if(*it == sock) it = r->userSockets.erase(it);
        else ++it;
    }

    // broadcast exit message with nickname
    Packet msgP;
    msgP.cmd = CMD_MSG;
    msgP.roomID = roomID;
    strcpy(msgP.id, "System");
    string noti = u->nickname + " left the chat.";
    strcpy(msgP.msg, noti.c_str());

    for (int s : r->userSockets) send_packet(s, msgP);

    // refresh userlist for others
    broadcast_room_users(roomID);

    // Room persists even if empty
}

void remove_user(int sock) {
    if (connectedUsers.find(sock) != connectedUsers.end()) {
        User* u = connectedUsers[sock];
        if (u->currentRoomID != -1) {
            leave_room_logic(sock, u->currentRoomID);
        }
        delete u;
        connectedUsers.erase(sock);
    }
}

void error_handling(string msg) {
    cerr << "[Error] " << msg << endl;
    exit(1);
}

// userlist refresh and send
void broadcast_room_users(int roomID) {
    // check if a room exists
    if (activeRooms.find(roomID) == activeRooms.end()) return;
    Room* r = activeRooms[roomID];

    // userlist (ex: "user1,user2,user3,")
    string userList = "";
    for (int s : r->userSockets) {
        if (connectedUsers.find(s) != connectedUsers.end()) {
            // send nickname instead of id
            userList += connectedUsers[s]->nickname + ",";
        }
    }

    Packet p;
    p.cmd = CMD_USER_LIST;
    strcpy(p.msg, userList.c_str());

    // new userlist send to everybody in room
    for (int s : r->userSockets) send_packet(s, p);
}
