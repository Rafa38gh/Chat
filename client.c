#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080

// Cores ANSI:
#define RESET   "\033[0m"
#define YELLOW  "\033[1;33m"

void* print_mensagens(void* arg)        // Todas as mensagens do client são recebidas aqui
{
    int sock = *(int*)arg;
    char buffer[1024];
    int n;

    while(1)
    {
        memset(buffer, 0, sizeof(buffer));
        n = recv(sock, buffer, sizeof(buffer)-1, 0);

        if(n > 0)
        {
            printf("%s", buffer);
            fflush(stdout);

        } else if(n == 0)
        {
            printf(YELLOW"Conexão encerrada pelo servidor.\n"RESET);
            break;

        } else
        {
            perror("Erro ao receber dados");
            break;
        }
    }
    return NULL;
}

void* input_mensagens(void* arg)        // Todas as mensagens do client são enviadas aqui
{
    int sock = *(int*)arg;
    char buffer[1024];

    while(1)
    {
        fgets(buffer, sizeof(buffer), stdin);
        if(buffer != NULL)
        {
            send(sock, buffer, strlen(buffer), 0);
        }
    }
    return NULL;
}

//=======================================================================================================================
int main()
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    pthread_t thread_mensagens;
    pthread_t thread_input;

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
        perror("Endereço inválido ou não suportado");
        exit(EXIT_FAILURE);
    }

    // Conectando ao servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Falha na conexão");
        exit(EXIT_FAILURE);
    }

    printf(YELLOW"Conexão estabelecida com sucesso!\n"RESET);

    // Thread print mensagens
    pthread_create(&thread_mensagens, NULL, print_mensagens, &sock);

    // Thread input mensagens
    pthread_create(&thread_input, NULL, input_mensagens, &sock);

    pthread_join(thread_mensagens, NULL);
    pthread_join(thread_input, NULL);

    close(sock);
    return 0;
}
