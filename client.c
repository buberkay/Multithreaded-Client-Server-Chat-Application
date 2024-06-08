#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void *receive_handler(void *socket_desc) {
    int sock = *(int *)socket_desc;
    char buffer[BUFFER_SIZE];
    int read_size;
    
    while ((read_size = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        printf("Alındı: %s", buffer);
        fflush(stdout);
    }

    if (read_size == 0) {
        printf("Sunucu bağlantısı kesildi\n");
    } else if (read_size == -1) {
        perror("Alma başarısız");
    }

    pthread_exit(NULL);
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Soket oluşturulamadı");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bağlanma başarısız");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Sunucuya bağlandı\n");

    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_handler, (void *)&sock) < 0) {
        perror("İş parçacığı oluşturulamadı");
        close(sock);
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("> ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            perror("Gönderme başarısız");
            close(sock);
            exit(EXIT_FAILURE);
        }

        if (strncmp(buffer, "LOGOUT", 6) == 0) {
            break;
        }
    }

    pthread_cancel(recv_thread);
    close(sock);
    return 0;
}