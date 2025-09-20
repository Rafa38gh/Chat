#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> // Adicionado para a funcionalidade de chat


#define PORT 8080
#define BUFFER_SIZE 2048


int connect_to_server() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    // Criando socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Falha ao criar socket");
        return -1; // Retorna -1 em caso de erro
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Converte IPv4 para binário
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Endereço inválido/Não suportado");
        close(sock);
        return -1;
    }

    // Conectando ao servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Falha na conexão");
        close(sock);
        return -1;
    }

    // Se tudo deu certo, retorna o socket conectado
    return sock;
}


void *receive_handler(void *socket_desc) {
    int sock = *(int *)socket_desc;
    char server_reply[BUFFER_SIZE];
    int read_size;

    // Fica em loop, esperando mensagens
    while ((read_size = recv(sock, server_reply, BUFFER_SIZE, 0)) > 0) {
        server_reply[read_size] = '\0'; // Adiciona o terminador de string
        printf("%s\n", server_reply);
        memset(server_reply, 0, BUFFER_SIZE); // Limpa o buffer
    }

    // Se o loop terminar, o servidor desconectou
    printf("Servidor desconectado. Pressione Enter para sair.\n");

    // Força o fechamento do socket para desbloquear o loop de envio na main
    shutdown(sock, SHUT_RDWR);

    return 0;
}


int main() {
    int sock = connect_to_server();

    // Se a conexão falhar (função retornou -1), termina o programa
    if (sock < 0) {
        fprintf(stderr, "Não foi possível conectar ao servidor.\n");
        return 1;
    }

    // Se conectou, imprime a mensagem original de sucesso
    printf("Conexão estabelecida com sucesso!\n");
    printf("Digite suas mensagens e pressione Enter para enviar.\n");

    // 2. Adiciona a funcionalidade de chat
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_handler, (void *)&sock) < 0) {
        perror("Nao foi possivel criar a thread de recebimento");
        close(sock);
        return 1;
    }

    // 3. Loop para ler a entrada do usuário e enviar (a outra parte do chat)
    char message[BUFFER_SIZE];
    while (fgets(message, BUFFER_SIZE, stdin) != NULL) {
        if (send(sock, message, strlen(message), 0) < 0) {
            perror("Falha no send");
            break;
        }
    }

    // 4. Limpeza
    close(sock);
    return 0;
}
