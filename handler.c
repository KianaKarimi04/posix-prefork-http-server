#include "handler.h"
#include "db.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>


int is_safe_path(const char *path) {
    return strstr(path, "..") == NULL;
}

void send_404(int client) {
    char *msg =
        "HTTP/1.0 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"
        "File not found\n";
    write(client, msg, strlen(msg));
}

void send_file(int client, const char *path, int head_only) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        send_404(client);
        return;
    }

    const char *type = "text/html";
    if (strstr(path, ".txt")) {
        type = "text/plain";
    }
    else if (strstr(path, ".jpg") || strstr(path, ".jpeg")) {
        type = "image/jpeg";
    }
    else if (strstr(path, ".png")) {
        type = "image/png";
    }
    else if (strstr(path, ".gif")) {
        type = "image/gif";
    }

    char header[256];
    sprintf(header,
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        type);
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

    body += 4;

    if (*body == '\0') {
        char *msg =
            "HTTP/1.0 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Empty POST body\n";
        write(client, msg, strlen(msg));
        return;
    }

    char key[50];
    sprintf(key, "%ld", time(NULL));

    // store_data(key, body);
    if (store_data(key, body) != 0) {
        char *msg = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
        write(client, msg, strlen(msg));
        return;
    }

    char *msg =
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"
        "POST stored\n";
    write(client, msg, strlen(msg));
}

void handle_client(int client) {
    char buffer[4096];
    int bytes = read(client, buffer, sizeof(buffer) - 1);

    if (bytes <= 0) {
        char *msg = "HTTP/1.0 400 Bad Request\r\n\r\n";
        write(client, msg, strlen(msg));
        return;
    }

    buffer[bytes] = '\0';

    char method[8], path[256];
    if (sscanf(buffer, "%s %s", method, path) != 2) {
        char *msg =
            "HTTP/1.0 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Bad Request\n";
        write(client, msg, strlen(msg));
        return;
    }

    // SECURITY: block path traversal
    if (!is_safe_path(path)) {
        send_404(client);
        return;
    }

    // default file
    if (strcmp(path, "/") == 0) {
        strcpy(path, "/index.html");
    }

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "www%s", path);

    if (strcmp(method, "GET") == 0) {
        send_file(client, full_path, 0);
    }
    else if (strcmp(method, "HEAD") == 0) {
        send_file(client, full_path, 1);
    }
    else if (strcmp(method, "POST") == 0) {
        handle_post(client, buffer);
    }
    else {
        // 405 instead of 400
        char *msg =
            "HTTP/1.0 405 Method Not Allowed\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Method Not Allowed\n";
        write(client, msg, strlen(msg));
    }
}