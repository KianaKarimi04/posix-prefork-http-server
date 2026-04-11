#include "handler.h"
#include "db.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

void send_404(int client) {
    char *msg =
        "HTTP/1.0 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "File not found";
    write(client, msg, strlen(msg));
}

void send_file(int client, const char *path, int head_only) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        send_404(client);
        return;
    }

    char header[256];
    sprintf(header,
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n");
    write(client, header, strlen(header));

    if (!head_only) {
        char buffer[1024];
        int bytes;
        while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
            write(client, buffer, bytes);
        }
    }

    close(fd);
}

void handle_post(int client, char *buffer) {
    char *body = strstr(buffer, "\r\n\r\n");
    if (!body) {
        char *msg =
            "HTTP/1.0 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n";
        write(client, msg, strlen(msg));
        return;
    }

    body += 4;  // ✅ ONLY ONCE

    char key[50];
    sprintf(key, "%ld", time(NULL));

    store_data(key, body);

    char *msg =
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "POST stored";
    write(client, msg, strlen(msg));
}

void handle_client(int client) {
    char buffer[4096];
    int bytes = read(client, buffer, sizeof(buffer) - 1);

    if (bytes <= 0) {
        char *msg =
            "HTTP/1.0 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n";
        write(client, msg, strlen(msg));
        return;
    }

    buffer[bytes] = '\0';

    char method[8], path[256];
    sscanf(buffer, "%7s %255s", method, path);  // ✅ safer

    char file_path[300];
    if (strcmp(path, "/") == 0) {
        strcpy(file_path, "www/index.html");
    } else {
        snprintf(file_path, sizeof(file_path), "www%s", path);
    }

    if (strcmp(method, "GET") == 0) {
        send_file(client, file_path, 0);
    }
    else if (strcmp(method, "HEAD") == 0) {
        send_file(client, file_path, 1);
    }
    else if (strcmp(method, "POST") == 0) {
        handle_post(client, buffer);
    }
    else {
        char *msg =
            "HTTP/1.0 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n";
        write(client, msg, strlen(msg));
    }
}