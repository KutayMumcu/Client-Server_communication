#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8888
#define BUFFER_SIZE 1024

void *receive_messages(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[BUFFER_SIZE];
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int len = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (len <= 0) {
            printf("Server closed the connection.\n");
            exit(0);
        }
        buffer[len] = '\0';
        printf("%s", buffer);
    }
    return NULL;
}

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t recv_thread;
    char input[BUFFER_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    pthread_create(&recv_thread, NULL, receive_messages, &sock);
    pthread_detach(recv_thread);

    while (1) {
        if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        if (strncmp(input, "exit", 4) == 0) {
            send(sock, input, strlen(input), 0);
            printf("You left the chat.\n");
            break;
        }
        send(sock, input, strlen(input), 0);
    }

    close(sock);
    return 0;
}
