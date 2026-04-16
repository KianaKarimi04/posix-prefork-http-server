#include <dlfcn.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080
#define SOURCE_LIB "./libhandler.so"

int server_fd;
time_t last_modified = 0;
void *lib_handle = NULL;

// Function pointer for the library function
void (*handle_client)(int) = NULL;

void load_handler() {
  struct stat attr;

  // 1. Check if the fresh build output exists
  if (stat(SOURCE_LIB, &attr) == -1) {
    return;
  }

  // 2. Only reload if the file timestamp has changed
  if (attr.st_mtime != last_modified) {
    last_modified = attr.st_mtime;

    printf("New version detected. Hot-reloading...\n");

    // Close the previous handle if it exists
    if (lib_handle) {
      dlclose(lib_handle);
      lib_handle = NULL;
      handle_client = NULL;
    }

    // 3. Create a unique filename for this version to bypass dlopen caching.
    // If we use the same name, the OS often returns the old memory map.
    char tmp_path[128];
    snprintf(tmp_path, sizeof(tmp_path), "./handler_tmp_%ld.so", last_modified);

    // 4. Copy the freshly built lib to the unique temporary path
    char copy_cmd[256];
    snprintf(copy_cmd, sizeof(copy_cmd), "cp %s %s", SOURCE_LIB, tmp_path);
    if (system(copy_cmd) != 0) {
      fprintf(stderr, "Failed to copy library for hot-reload\n");
      return;
    }

    // 5. Load the UNIQUE temporary file
    void *new_handle = dlopen(tmp_path, RTLD_NOW);
    if (!new_handle) {
      fprintf(stderr, "dlopen error: %s\n", dlerror());
      unlink(tmp_path);
      return;
    }

    // 6. Resolve the symbol
    void (*new_func)(int) = dlsym(new_handle, "handle_client");
    if (!new_func) {
      fprintf(stderr, "dlsym error: %s\n", dlerror());
      dlclose(new_handle);
      unlink(tmp_path);
      return;
    }

    lib_handle = new_handle;
    handle_client = new_func;

    // 7. Cleanup: Delete the temp file from disk.
    // The OS keeps the code in memory as long as the handle is open.
    unlink(tmp_path);
    printf("Successfully loaded new handler version!\n");
  }
}

void worker_loop() {
  while (1) {
    int client = accept(server_fd, NULL, NULL);
    if (client < 0) {
      perror("accept");
      continue;
    }

    // Check for updates every time a request comes in
    load_handler();

    if (!handle_client) {
      const char *msg = "HTTP/1.0 500 Internal Server Error\r\n"
                        "Content-Type: text/plain\r\n\r\n"
                        "Handler not loaded\n";
      write(client, msg, strlen(msg));
      close(client);
      continue;
    }

    printf("Worker %d handling request\n", getpid());
    handle_client(client);
    close(client);
  }
}

void handle_sigint(int sig) {
  (void)sig;
  printf("\nShutting down server...\n");
  if (lib_handle) {
    dlclose(lib_handle);
  }
  close(server_fd);
  exit(0);
}

int main() {
  signal(SIGINT, handle_sigint);

  struct sockaddr_in addr;
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    exit(1);
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    exit(1);
  }

  if (listen(server_fd, 10) < 0) {
    perror("listen");
    exit(1);
  }

  printf("Server running on port %d...\n", PORT);

  // Initial load attempt
  load_handler();

  if (!handle_client) {
    fprintf(stderr, "Critical: Failed to load handler at startup\n");
    exit(1);
  }

  worker_loop();
  return 0;
}