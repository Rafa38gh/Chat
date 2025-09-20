#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h> // Adicionado para gerenciar múltiplos clientes

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 2048

int client_sockets[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int setup_server_socket() {
    int sockfd;
    struct sockaddr_in endereco;

    // Criando socket (do seu código original)
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Falha ao criar socket");
        return -1;
    }

    // Configurando socket (do seu código original)
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("Falha ao definir socket");
        close(sockfd);
        return -1;
    }

    // Definindo endereço do server (do seu código original)
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORT);
    endereco.sin_addr.s_addr = INADDR_ANY;

    // Conectando socket na porta (do seu código original)
    if (bind(sockfd, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
        perror("Falha no bind");
        close(sockfd);
        return -1;
    }

    // Listen (do seu código original)
    if (listen(sockfd, 3) < 0) {
        perror("Falha no listen");
        close(sockfd);
        return -1;
    }

    // Se tudo deu certo, retorna o socket pronto para aceitar conexões
    return sockfd;
}


void broadcast_message(char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] != sender_socket) {
            send(client_sockets[i], message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Função que cada thread de cliente executará
void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    char buffer[BUFFER_SIZE];
    int read_size;

    while ((read_size = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        char message_to_send[BUFFER_SIZE + 20];
        sprintf(message_to_send, "Cliente %d: %s", client_socket, buffer);
        broadcast_message(message_to_send, client_socket);
        memset(buffer, 0, BUFFER_SIZE);
    }

    // Lógica de desconexão
    printf("Cliente %d desconectou.\n", client_socket);
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] == client_socket) {
            client_sockets[i] = client_sockets[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(client_socket);
    free(arg);
    return NULL;
}


int main() {
    // 1. Usa a sua função original para configurar o servidor
    int sockfd = setup_server_socket();

    if (sockfd < 0) {
        fprintf(stderr, "Falha ao iniciar o servidor.\n");
        return 1;
    }

    printf("Servidor de Chat aguardando conexões na porta %d...\n", PORT);

    // 2. Loop principal para aceitar novas conexões
    // O accept() que estava no seu main original agora está dentro deste loop
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int novo_socket = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);

        if (novo_socket < 0) {
            perror("Falha no accept");
            continue;
        }

        // Lógica para adicionar o novo cliente e criar sua thread
        pthread_mutex_lock(&clients_mutex);
        if (client_count < MAX_CLIENTS) {
            client_sockets[client_count++] = novo_socket;
            pthread_mutex_unlock(&clients_mutex);

            printf("Nova conexão aceita! Socket: %d\n", novo_socket);

            pthread_t thread_id;
            int *p_socket = malloc(sizeof(int));
            *p_socket = novo_socket;

            if (pthread_create(&thread_id, NULL, handle_client, (void *)p_socket) < 0) {
                perror("Nao foi possivel criar a thread");
            }
        } else {
            // Se o servidor estiver cheio
            pthread_mutex_unlock(&clients_mutex);
            printf("Conexão recusada. Servidor cheio.\n");
            send(novo_socket, "Servidor cheio.\n", strlen("Servidor cheio.\n"), 0);
            close(novo_socket);
        }
    }

    close(sockfd);
    return 0;
}
