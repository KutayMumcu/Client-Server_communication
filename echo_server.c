/* ============================ */
/*        echo_server.c        */
/* ============================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 3
#define SESSION_DURATION 30

int client_count = 0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

int clients[MAX_CLIENTS];
int client_ids[MAX_CLIENTS];

void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    char buffer[BUFFER_SIZE];
    struct timeval start, current;
    gettimeofday(&start, NULL);

    while (1) {
        gettimeofday(&current, NULL);
        int elapsed = current.tv_sec - start.tv_sec;
        if (elapsed >= SESSION_DURATION) {
            break;
        }

        memset(buffer, 0, BUFFER_SIZE);
        int read_size = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (read_size <= 0) break;

        // Broadcast to other clients
        pthread_mutex_lock(&client_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] != 0 && clients[i] != client_fd) {
                char msg[BUFFER_SIZE + 10];
                snprintf(msg, sizeof(msg), "#%d: %s", client_fd, buffer);
                send(clients[i], msg, strlen(msg), 0);
            }
        }
        pthread_mutex_unlock(&client_mutex);
    }

    close(client_fd);
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == client_fd) {
            clients[i] = 0;
            client_ids[i] = 0;
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);
    return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, MAX_CLIENTS);

    printf("Chat Server started on port %d\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        pthread_mutex_lock(&client_mutex);
        if (client_count >= MAX_CLIENTS) {
            printf("Rejected connection - max clients reached\n");
            close(client_fd);
            pthread_mutex_unlock(&client_mutex);
            continue;
        }

        int idx;
        for (idx = 0; idx < MAX_CLIENTS; idx++) {
            if (clients[idx] == 0) {
                clients[idx] = client_fd;
                client_ids[idx] = client_fd;
                break;
            }
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, &clients[idx]);
        pthread_detach(tid);
        client_count++;
        pthread_mutex_unlock(&client_mutex);
    }

    close(server_fd);
    return 0;
}
