#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h> // sock_addr_in
#include <pthread.h>
#include <time.h>


#define PORT 8080

void* horario_atual(void* arg)
{
    int sock = *(int*)arg;
    char msg[64];
    char horario[16];
    time_t agora;
    struct tm *info_hora;

    while(1)
    {
        agora = time(NULL);
        info_hora = localtime(&agora);
        info_hora->tm_hour -= 3; // UTC-3

        strftime(horario, sizeof(horario), "%H:%M:%S\n", info_hora);
        snprintf(msg, sizeof(msg), "Horário Atual: %s", horario);

        send(sock, msg, strlen(msg), 0);
        sleep(60);
    }
} 

//===================================================================================================================
int main(int argc, char *argv[])
{
    // Variáveis
    //======================================================================
    //int opt = 1;
    int sockfd, novo_socket;
    struct sockaddr_in endereco;
    int addrlen = sizeof(endereco);
    pthread_t thread_horario;

    //======================================================================
    
    // Criando socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        perror("Falha ao criar socket");
        exit(EXIT_FAILURE);
    }

    //setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    /*if(setsockopt < 0)
    {
        perror("Falha ao definir socket");
        exit(EXIT_FAILURE);
    }*/

    // Definindo endereço do server
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORT);
    endereco.sin_addr.s_addr = INADDR_ANY;
    
    // Conectando socket na porta
    bind(sockfd, (struct sockaddr *)&endereco, sizeof(endereco));
    if(bind < 0)
    {
        perror("Falha no bind");
        exit(EXIT_FAILURE);
    }

    // Listen
    listen(sockfd, 3);
    if(listen < 0)
    {
        perror("Falha no listen");
        exit(EXIT_FAILURE);
    }

    // Aceitar conexões
    novo_socket = accept(sockfd, (struct sockaddr *)&endereco, (socklen_t*)&addrlen);
    if(novo_socket < 0)
    {
        perror("Falha no accept");
        exit(EXIT_FAILURE);
    }

    // Thread para enviar horário
    pthread_create(&thread_horario, NULL, horario_atual, &novo_socket);

    pthread_join(thread_horario, NULL);
    
    return 0;
}