#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 3

typedef struct {
    int fd;         // socket fd
    int slot;       // 0..MAX_CLIENTS-1
    int user_id;    
} client_t;

client_t *clients[MAX_CLIENTS] = {0};
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int next_user_id = 1;

void broadcast_message(const char *msg, size_t len, client_t *exclude) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i] != exclude) {
            send(clients[i]->fd, msg, len, 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(client_t *cli) {
    pthread_mutex_lock(&clients_mutex);
    clients[cli->slot] = NULL;
    pthread_mutex_unlock(&clients_mutex);
    close(cli->fd);
    free(cli);
}

void *handle_client(void *arg) {
    client_t *cli = (client_t *)arg;
    char buffer[BUFFER_SIZE];

    char welcome[BUFFER_SIZE];
    snprintf(welcome, sizeof(welcome),
             "Welcome to the chat, User %d! Type 'exit' to leave.\n",
             cli->user_id);
    send(cli->fd, welcome, strlen(welcome), 0);

    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, sizeof(join_msg),
             "User %d has joined the chat.\n",
             cli->user_id);
    broadcast_message(join_msg, strlen(join_msg), cli);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int r = recv(cli->fd, buffer, BUFFER_SIZE - 1, 0);
        if (r <= 0) break;
        buffer[r] = '\0';

        if (strncmp(buffer, "exit", 4) == 0 &&
            (buffer[4] == '\n' || buffer[4] == '\0')) {
            break;
        }

        char outbuf[BUFFER_SIZE + 32];
        int n = snprintf(outbuf, sizeof(outbuf),
                         "User %d: %s", cli->user_id, buffer);
        broadcast_message(outbuf, n, cli);
    }

    char leave_msg[BUFFER_SIZE];
    snprintf(leave_msg, sizeof(leave_msg),
             "User %d has left the chat.\n", cli->user_id);
    broadcast_message(leave_msg, strlen(leave_msg), cli);

    remove_client(cli);
    pthread_exit(NULL);
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Chat server listening on port %d (max %d users)…\n", PORT, MAX_CLIENTS);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(server_fd,
                               (struct sockaddr*)&client_addr,
                               &len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i]) {
                slot = i;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (slot < 0) {
            const char *full = "Server is full. Try again later.\n";
            send(client_fd, full, strlen(full), 0);
            close(client_fd);
            continue;
        }

        client_t *cli = malloc(sizeof(client_t));
        cli->fd      = client_fd;
        cli->slot    = slot;
        cli->user_id = next_user_id++;

        pthread_mutex_lock(&clients_mutex);
        clients[slot] = cli;
        pthread_mutex_unlock(&clients_mutex);

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, cli);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
