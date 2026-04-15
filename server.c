#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 8080
#define WORKERS 5

int server_fd;
time_t last_modified = 0;
void *lib_handle = NULL;

// function pointer
void (*handle_client)(int) = NULL;

void load_handler() {
    struct stat attr;

    if (stat("./handler.so", &attr) == -1) {
        perror("stat");
        exit(1);
    }

    // reload if modified
    if (attr.st_mtime != last_modified) {
        last_modified = attr.st_mtime;

        if (lib_handle) {
            dlclose(lib_handle);
        }

        usleep(100000);  // 0.1 sec delay

        dlerror();  // clear old errors
        lib_handle = dlopen("./handler.so", RTLD_NOW | RTLD_GLOBAL);
        if (!lib_handle) {
            fprintf(stderr, "dlopen error: %s\n", dlerror());
            exit(1);
        }

        handle_client = dlsym(lib_handle, "handle_client");
        if (!handle_client) {
            fprintf(stderr, "dlsym error: %s\n", dlerror());
            exit(1);
        }

        printf("Reloaded handler.so\n");
    }
}

void create_workers() {
    for (int i = 0; i < WORKERS; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) { // child
            while (1) {
                load_handler();

                int client = accept(server_fd, NULL, NULL);
                if (client < 0) {
                    perror("accept");
                    continue;
                }

                printf("Worker %d handling request\n", getpid());

                if (handle_client) {
                    handle_client(client);
                }

                close(client);
            }
            exit(0);
        }
    }
}

void monitor_workers() {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Worker %d died. Restarting...\n", pid);

        if (fork() == 0) {
            while (1) {
                load_handler();

                int client = accept(server_fd, NULL, NULL);
                if (client < 0) {
                    perror("accept");
                    continue;
                }

                printf("Worker %d handling request\n", getpid());

                if (handle_client) {
                    handle_client(client);
                }

                close(client);
            }
            exit(0);
        }
    }
}

void handle_sigint(int sig) {
    printf("\nShutting down server...\n");
    close(server_fd);
    exit(0);
}

int main() {
    signal(SIGINT, handle_sigint);

    struct sockaddr_in addr;

    // create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    // allow quick restart
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // setup address
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    // bind
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    // listen
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Server running on port %d...\n", PORT);

    // load handler before forking
    load_handler();

    // create workers
    create_workers();

    // parent monitors workers
    while (1) {
        monitor_workers();
        sleep(1);
    }

    return 0;
}