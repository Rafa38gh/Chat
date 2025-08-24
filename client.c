#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main() 
{
    int sock = 0;
    struct sockaddr_in serv_addr;

    // Criando socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("Falha ao criar socket");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Converte IPv4 para binário
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) 
    {
        perror("Endereço inválido/Não suportado");
        exit(EXIT_FAILURE);
    }

    // Conectando ao servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    {
        perror("Falha na conexão");
        exit(EXIT_FAILURE);
    }

    printf("Conexão estabelecida com sucesso!\n");

    close(sock);
    return 0;
}