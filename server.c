#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

typedef struct {
    char username[50];
    char password[50];
    char name[50];
    char surname[50];
    char mood[50];
    bool online;
    int socket;
} User;

User users[MAX_CLIENTS];
int user_count = 0;

pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

void load_users() {
    FILE *file = fopen("users.txt", "r");
    if (file == NULL) {
        perror("Kullanıcı dosyası açılamadı");
        exit(EXIT_FAILURE);
    }

    while (fscanf(file, "%s %s %s %s %s", users[user_count].username, users[user_count].password, users[user_count].name, users[user_count].surname, users[user_count].mood) != EOF) {
        users[user_count].online = false;
        user_count++;
    }

    fclose(file);
}

void save_users() {
    FILE *file = fopen("users.txt", "w");
    if (file == NULL) {
        perror("Kullanıcı dosyası açılamadı");
        return;
    }

    for (int i = 0; i < user_count; i++) {
        fprintf(file, "%s %s %s %s %s\n", users[i].username, users[i].password, users[i].name, users[i].surname, users[i].mood);
    }

    fclose(file);
}

void *handle_client(void *arg) {
    int new_socket = *((int *)arg);
    free(arg);
    char buffer[BUFFER_SIZE];
    char username[50];
    bool logged_in = false;
    int user_index = -1;

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int read_size = recv(new_socket, buffer, BUFFER_SIZE, 0);
        if (read_size <= 0) {
            if (logged_in && user_index != -1) {
                pthread_mutex_lock(&users_mutex);
                users[user_index].online = false;
                pthread_mutex_unlock(&users_mutex);
            }
            close(new_socket);
            pthread_exit(NULL);
        }

        char command[BUFFER_SIZE];
        sscanf(buffer, "%s", command);

        if (strcmp(command, "REGISTER") == 0) {
            char reg_username[50], reg_password[50], reg_name[50], reg_surname[50], reg_mood[50];
            sscanf(buffer, "%*s %s %s %s %s %s", reg_username, reg_password, reg_name, reg_surname, reg_mood);
            pthread_mutex_lock(&users_mutex);
            strcpy(users[user_count].username, reg_username);
            strcpy(users[user_count].password, reg_password);
            sprintf(users[user_count].name, "%s %s", reg_name, reg_surname);
            strcpy(users[user_count].mood, reg_mood);
            users[user_count].online = false;
            user_count++;
            save_users();
            pthread_mutex_unlock(&users_mutex);
            send(new_socket, "Kayıt başarılı\n", strlen("Kayıt başarılı\n"), 0);
        } else if (strcmp(command, "LOGIN") == 0) {
            char login_username[50], login_password[50], login_mood[50];
            sscanf(buffer, "%*s %s %s %s", login_username, login_password, login_mood);
            pthread_mutex_lock(&users_mutex);
            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].username, login_username) == 0 && strcmp(users[i].password, login_password) == 0) {
                    users[i].online = true;
                    users[i].socket = new_socket;
                    strcpy(users[i].mood, login_mood);
                    logged_in = true;
                    user_index = i;
                    strcpy(username, login_username);
                    send(new_socket, "Giriş başarılı\n", strlen("Giriş başarılı\n"), 0);
                    break;
                }
            }
            pthread_mutex_unlock(&users_mutex);
            if (!logged_in) {
                send(new_socket, "Giriş başarısız\n", strlen("Giriş başarısız\n"), 0);
            }
        } else if (strcmp(command, "LOGOUT") == 0) {
            if (logged_in && user_index != -1) {
                pthread_mutex_lock(&users_mutex);
                users[user_index].online = false;
                pthread_mutex_unlock(&users_mutex);
                logged_in = false;
                send(new_socket, "Çıkış başarılı\n", strlen("Çıkış başarılı\n"), 0);
                break;
            } else {
                send(new_socket, "Giriş yapılmadı\n", strlen("Giriş yapılmadı\n"), 0);
            }
        } else if (strcmp(command, "LIST") == 0) {
            char response[BUFFER_SIZE] = "Kullanıcılar:\n";
            pthread_mutex_lock(&users_mutex);
            for (int i = 0; i < user_count; i++) {
                char user_info[100];
                snprintf(user_info, sizeof(user_info), "%s (%s)\n", users[i].username, users[i].online ? "çevrimiçi" : "çevrimdışı");
                strcat(response, user_info);
            }
            pthread_mutex_unlock(&users_mutex);
            send(new_socket, response, strlen(response), 0);
        } else if (strcmp(command, "MSG") == 0) {
            char target_user[50], message[BUFFER_SIZE];
            sscanf(buffer, "%*s %s %[^\n]", target_user, message);
            if (strcmp(target_user, "*") == 0) {
                pthread_mutex_lock(&users_mutex);
                for (int i = 0; i < user_count; i++) {
                    if (users[i].online && users[i].socket != new_socket) {
                        char msg[BUFFER_SIZE];
                        snprintf(msg, sizeof(msg), "%s: %s\n", username, message);
                        send(users[i].socket, msg, strlen(msg), 0);
                    }
                }
                pthread_mutex_unlock(&users_mutex);
            } else {
                pthread_mutex_lock(&users_mutex);
                for (int i = 0; i < user_count; i++) {
                    if (strcmp(users[i].username, target_user) == 0 && users[i].online) {
                        char msg[BUFFER_SIZE];
                        snprintf(msg, sizeof(msg), "%s: %s\n", username, message);
                        send(users[i].socket, msg, strlen(msg), 0);
                        break;
                    }
                }
                pthread_mutex_unlock(&users_mutex);
            }
        } else if (strcmp(command, "INFO") == 0) {
            char target_user[50];
            sscanf(buffer, "%*s %s", target_user);
            pthread_mutex_lock(&users_mutex);
            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].username, target_user) == 0) {
                    char info[BUFFER_SIZE];
                    snprintf(info, sizeof(info), "İsim: %s\nRuh Hali: %s\n", users[i].name, users[i].mood);
                    send(new_socket, info, strlen(info), 0);
                    break;
                }
            }
            pthread_mutex_unlock(&users_mutex);
        } else {
            send(new_socket, "Bilinmeyen komut\n", strlen("Bilinmeyen komut\n"), 0);
        }

        fflush(stdout);
        memset(buffer, 0, BUFFER_SIZE);
    }

    close(new_socket);
    pthread_exit(NULL);
}

int main() {
    int server_socket, new_socket, *new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Soket oluşturulamadı");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bağlantı başarısız");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    listen(server_socket, 3);

    load_users();

    printf("Bağlantılar bekleniyor...\n");
    while ((new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len))) {
        printf("Bağlantı kabul edildi\n");

        pthread_t client_thread;
        new_sock = malloc(1);
        *new_sock = new_socket;

        if (pthread_create(&client_thread, NULL, handle_client, (void *)new_sock) < 0) {
            perror("İş parçacığı oluşturulamadı");
            close(new_socket);
            free(new_sock);
        }

        pthread_detach(client_thread);
    }

    if (new_socket < 0) {
        perror("Kabul başarısız");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    close(server_socket);
    return 0;
}

