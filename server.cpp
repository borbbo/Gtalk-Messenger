#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include "protocol.h" // Must be in the same directory

#define EPOLL_SIZE 50

// Function Declarations
void send_msg(Packet packet, int len);
void error_handling(const char *buf);

// Global Variables
struct epoll_event *ep_events;
struct epoll_event event;
int epoll_fd;
int client_socks[MAX_CLNT]; // Array to manage connected client sockets
int clnt_cnt = 0;           // Current number of connected clients

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t adr_sz;
    int str_len, i;
    int event_cnt;
    Packet packet;

    // 1. Create Server Socket
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(PORT); // Defined in protocol.h (9999)

    // 2. Bind Address
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    // 3. Listen for Connections
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // 4. Create Epoll Instance
    epoll_fd = epoll_create(EPOLL_SIZE);

    // ** Important for C++: Explicit casting for malloc **
    ep_events = (struct epoll_event*)malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

    // Register server socket to epoll monitoring
    event.events = EPOLLIN; // Monitor for incoming data (read)
    event.data.fd = serv_sock;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serv_sock, &event);

    printf("Server started on port %d...\n", PORT);

    while (1)
    {
        // Wait for events (Blocking)
        event_cnt = epoll_wait(epoll_fd, ep_events, EPOLL_SIZE, -1);

        if (event_cnt == -1) {
            puts("epoll_wait() error");
            break;
        }

        for (i = 0; i < event_cnt; i++)
        {
            if (ep_events[i].data.fd == serv_sock) // Case A: New Connection Request
            {
                adr_sz = sizeof(clnt_adr);
                clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &adr_sz);

                // Register new client to epoll
                event.events = EPOLLIN;
                event.data.fd = clnt_sock;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clnt_sock, &event);

                client_socks[clnt_cnt++] = clnt_sock; // Add to management array
                printf("Connected client: %d \n", clnt_sock);
            }
            else // Case B: Data Received from Client
            {
                str_len = read(ep_events[i].data.fd, &packet, sizeof(Packet));

                if (str_len == 0) // Client Disconnected
                {
                    // Remove from epoll
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
                    close(ep_events[i].data.fd);
                    printf("Closed client: %d \n", ep_events[i].data.fd);
                    // Note: Removing from 'client_socks' array is omitted for simplicity
                }
                else
                {
                    // Broadcast message to all clients (except me)
                    for (int j = 0; j < clnt_cnt; j++) {
                        if (client_socks[j] != ep_events[i].data.fd) // except sender
                            write(client_socks[j], &packet, str_len);
                    }

                    // print out server log
                    if(packet.cmd == CMD_LOGIN) printf("[LOGIN] %s\n", packet.id);
                    else if(packet.cmd == CMD_FILE) printf("[FILE] From %s: %s\n", packet.id, packet.fileName);

                }
            }
        }
    }

    close(serv_sock);
    close(epoll_fd);
    free(ep_events);
    return 0;
}

// Function to send message to all connected clients
void send_msg(Packet packet, int len)
{
    int i;
    for (i = 0; i < clnt_cnt; i++)
        write(client_socks[i], &packet, len);
}

void error_handling(const char *buf)
{
    fputs(buf, stderr);
    fputc('\n', stderr);
    exit(1);
}
