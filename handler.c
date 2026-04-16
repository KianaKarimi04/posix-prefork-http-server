#include "handler.h"
#include "db.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int is_safe_path(const char *path) { return strstr(path, "..") == NULL; }

void send_404(int client) {
  char *msg = "HTTP/1.0 404 Not Found\r\n"
              "Content-Type: text/plain\r\n"
              "Connection: close\r\n\r\n"
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
  if (strstr(path, ".txt"))
    type = "text/plain";
  else if (strstr(path, ".jpg") || strstr(path, ".jpeg"))
    type = "image/jpeg";
  else if (strstr(path, ".png"))
    type = "image/png";
  else if (strstr(path, ".gif"))
    type = "image/gif";

  char header[256];
  snprintf(header, sizeof(header),
           "HTTP/1.0 200 OK\r\n"
           "Content-Type: %s\r\n"
           "Connection: close\r\n\r\n",
           type);

  write(client, header, strlen(header));

  if (!head_only) {
    char buffer[1024];
    ssize_t bytes;
    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
      write(client, buffer, bytes);
    }
  }

  close(fd);
}

void handle_post(int client, char *buffer) {
  char *body = strstr(buffer, "\r\n\r\n");
  if (!body) {
    write(client, "HTTP/1.0 400 Bad Request\r\n\r\n", 28);
    return;
  }

  body += 4;

  if (strlen(body) == 0) {
    write(client, "HTTP/1.0 400 Bad Request\r\n\r\n", 28);
    return;
  }

  char key[50];
  snprintf(key, sizeof(key), "%ld", time(NULL));

  if (store_data(key, body) != 0) {
    write(client, "HTTP/1.0 500 Internal Server Error\r\n\r\n", 36);
    return;
  }

  // write(client,
  //       "HTTP/1.0 200 OK\r\n"
  //       "Content-Type: text/plain\r\n"
  //       "Connection: close\r\n\r\n"
  //       "POST stored\n",
  //       86);
  const char *msg = "HTTP/1.0 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    "Connection: close\r\n\r\n"
                    "it works!!!\n";

  write(client, msg, strlen(msg));
}

void handle_client(int client) {
  char buffer[4096];
  int total = 0;
  ssize_t bytes;

  // read headers
  while ((bytes = read(client, buffer + total, sizeof(buffer) - 1 - total)) >
         0) {
    total += bytes;
    buffer[total] = '\0';

    if (strstr(buffer, "\r\n\r\n"))
      break;
  }

  if (total <= 0) {
    write(client, "HTTP/1.0 400 Bad Request\r\n\r\n", 28);
    return;
  }

  int content_length = 0;
  char *cl = strstr(buffer, "Content-Length:");
  if (cl) {
    sscanf(cl, "Content-Length: %d", &content_length);
  }

  char *body_start = strstr(buffer, "\r\n\r\n");
  if (!body_start) {
    write(client, "HTTP/1.0 400 Bad Request\r\n\r\n", 28);
    return;
  }

  body_start += 4;
  int body_received = total - (body_start - buffer);

  while (body_received < content_length) {
    bytes = read(client, buffer + total, sizeof(buffer) - 1 - total);
    if (bytes <= 0)
      break;

    total += bytes;
    body_received += bytes;
  }

  buffer[total] = '\0';

  char method[8], path[256];
  if (sscanf(buffer, "%7s %255s", method, path) != 2) {
    write(client, "HTTP/1.0 400 Bad Request\r\n\r\n", 28);
    return;
  }

  if (!is_safe_path(path)) {
    send_404(client);
    return;
  }

  if (strcmp(path, "/") == 0) {
    strcpy(path, "/index.html");
  }

  char full_path[512];
  snprintf(full_path, sizeof(full_path), "../www%s", path);

  if (strncmp(method, "GET", 3) == 0) {
    send_file(client, full_path, 0);
  } else if (strncmp(method, "HEAD", 4) == 0) {
    send_file(client, full_path, 1);
  } else if (strncmp(method, "POST", 4) == 0) {
    handle_post(client, buffer);
  } else {
    write(client, "HTTP/1.0 405 Method Not Allowed\r\n\r\n", 36);
  }
}
