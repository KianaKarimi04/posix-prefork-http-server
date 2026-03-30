//
// Created by kiana on 3/29/26.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#define PORT 8080
#define WORKERS 5

int server_fd;

void create_workers() {
    for (int i = 0; i < WORKERS; i++) {
        if (fork() == 0) {
            while (1) {
                int client = accept(server_fd, NULL, NULL);
                if (client >= 0) {
                    handle_client(client);
                    close(client);
                }
            }
            exit(0);
        }
    }
}

int main() {
    struct sockaddr_in addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    create_workers();

    while (1) pause(); // keep parent alive
}