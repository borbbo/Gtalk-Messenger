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

// [Data Structure: User]
struct User {
    int socket;
    string id;
    int currentRoomID; // -1: Lobby, >=0: Specific Room ID
    bool isLoggedIn;
};

// [Data Structure: Room]
struct Room {
    int id;
    string title;
    vector<int> userSockets; // List of sockets in this room
};

// [Global Variables]
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
                // [New Connection]
                struct sockaddr_in clnt_adr;
                socklen_t adr_sz = sizeof(clnt_adr);
                int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);

                event.events = EPOLLIN;
                event.data.fd = clnt_sock;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clnt_sock, &event);

                // Create temporary User object (Not logged in yet)
                User* newUser = new User{clnt_sock, "", -1, false};
                connectedUsers[clnt_sock] = newUser;
                cout << "[System] Client connected: " << clnt_sock << endl;
            }
            else {
                // [Data Received]
                Packet p;
                int str_len = read(cur_fd, &p, sizeof(Packet));

                if (str_len <= 0) {
                    // [Disconnection]
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cur_fd, NULL);
                    close(cur_fd);
                    remove_user(cur_fd); // Clean up user data
                    cout << "[System] Client disconnected: " << cur_fd << endl;
                }
                else {
                    // [Process Packet]
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
            newRoom->userSockets.push_back(sock);

            activeRooms[newRoom->id] = newRoom;
            u->currentRoomID = newRoom->id;

            // Notify User
            res.cmd = CMD_JOIN_ROOM;
            res.roomID = newRoom->id;
            strcpy(res.msg, newRoom->title.c_str());
            send_packet(sock, res);

            cout << "[Room] Created: " << newRoom->title << " (ID: " << newRoom->id << ")" << endl;
            break;
        }

        // [4] Join Room (Simple implementation: By Room ID)
        case CMD_JOIN_ROOM: {
             if (!u->isLoggedIn) return;
             int targetRoomID = p.roomID;

             if (activeRooms.find(targetRoomID) != activeRooms.end()) {
                 Room* r = activeRooms[targetRoomID];
                 r->userSockets.push_back(sock);
                 u->currentRoomID = targetRoomID;

                 res.cmd = CMD_JOIN_ROOM;
                 res.roomID = r->id;
                 strcpy(res.msg, r->title.c_str());
                 send_packet(sock, res);
                 cout << "[Room] " << u->id << " joined Room " << targetRoomID << endl;
             }
             break;
        }

        // [5] Room List Request (For Lobby)
        case CMD_ROOM_LIST: {
             string listStr = "";
             for(auto const& [key, val] : activeRooms) {
                 // Format: "ID:Title,"
                 listStr += to_string(key) + ":" + val->title + ",";
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
                 for (int s : r->userSockets) {
                     if (s != sock) send_packet(s, p); // Don't echo back to sender
                 }
                 cout << "[File] " << u->id << " sent a file." << endl;
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

void remove_user(int sock) {
    if (connectedUsers.find(sock) != connectedUsers.end()) {
        User* u = connectedUsers[sock];
        // Remove from current room if any
        if (u->currentRoomID != -1 && activeRooms.find(u->currentRoomID) != activeRooms.end()) {
            Room* r = activeRooms[u->currentRoomID];
            // Erase-remove idiom for vector
            r->userSockets.erase(std::remove(r->userSockets.begin(), r->userSockets.end(), sock), r->userSockets.end());
        }
        delete u;
        connectedUsers.erase(sock);
    }
}

void error_handling(string msg) {
    cerr << "[Error] " << msg << endl;
    exit(1);
}
