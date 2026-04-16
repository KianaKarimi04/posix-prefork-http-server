#include <dlfcn.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080
#define WORKERS 4 // Change this to match the number of workers in your test
#define SOURCE_LIB "./libhandler.so"

int server_fd;
time_t last_modified = 0;
void *lib_handle = NULL;
void (*handle_client)(int) = NULL;

// Hot-reload logic: Unique naming ensures we bypass the OS linker cache
void load_handler() {
  struct stat attr;
  if (stat(SOURCE_LIB, &attr) == -1)
    return;

  if (attr.st_mtime != last_modified) {
    last_modified = attr.st_mtime;
    if (lib_handle) {
      dlclose(lib_handle);
      lib_handle = NULL;
    }

    char tmp_path[128];
    snprintf(tmp_path, sizeof(tmp_path), "./handler_v%ld.so", last_modified);
    char copy_cmd[256];
    snprintf(copy_cmd, sizeof(copy_cmd), "cp %s %s", SOURCE_LIB, tmp_path);

    if (system(copy_cmd) == 0) {
      lib_handle = dlopen(tmp_path, RTLD_NOW);
      if (lib_handle) {
        handle_client = dlsym(lib_handle, "handle_client");
        printf("Successfully loaded handler version: %ld\n", last_modified);
      }
      unlink(tmp_path); // Remove temp file; the code remains in process memory
    }
  }
}

// Worker logic: Each child process runs this loop
void worker_process() {
  while (1) {
    int client = accept(server_fd, NULL, NULL);
    if (client < 0)
      continue;

    // Reload check happens inside the worker on every request
    load_handler();

    if (handle_client) {
      handle_client(client);
    } else {
      const char *err =
          "HTTP/1.0 500 Internal Server Error\r\n\r\nHandler Missing";
      write(client, err, strlen(err));
    }
    close(client);
  }
}

// Spawn a worker using fork()
pid_t spawn_worker() {
  pid_t pid = fork();
  if (pid == 0) {
    worker_process();
    exit(0); // Should never reach here unless loop breaks
  }
  return pid;
}

void handle_sigint(int sig) {
  (void)sig;
  printf("\nShutting down server and killing all workers...\n");
  kill(0, SIGTERM); // Kill the entire process group
  close(server_fd);
  exit(0);
}

int main() {
  signal(SIGINT, handle_sigint);

  struct sockaddr_in addr = {0};
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    exit(1);
  }
  listen(server_fd, 10);
  printf("Server running on port %d. Monitoring %d workers...\n", PORT,
         WORKERS);

  load_handler(); // Initial load for the parent

  // Initial Prefork
  for (int i = 0; i < WORKERS; i++) {
    spawn_worker();
  }

  // Monitor Loop: Parent waits for children to die and restarts them
  while (1) {
    int status;
    pid_t dead_worker = wait(&status);
    if (dead_worker > 0) {
      printf("Worker %d died. Restarting...\n", dead_worker);
      spawn_worker();
    }
  }

  return 0;
}