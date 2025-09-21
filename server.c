#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h> // sock_addr_in
#include <pthread.h>
#include <time.h>
#include "fila.h"

#define PORT 8080
#define MAX_CLIENTS 10

// ==============================================================
// Informações dos clientes
typedef struct cliente
{
    int socket;
    char nome[50];
    pthread_t thread_envia;     // Envia mensagens para o client
    pthread_t thread_processa;  // Processa mensagens do client
} Cliente;

// Lista global de clientes
Cliente* clientes[MAX_CLIENTS];
int num_clientes = 0;
int limite_clientes = 0;
pthread_mutex_t clientes_lock = PTHREAD_MUTEX_INITIALIZER;

// Fila de mensagens
Fila* fila_mensagens;
pthread_mutex_t fila_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fila_cond = PTHREAD_COND_INITIALIZER;

/* ==============================================================
// Clientes
=================================================================*/

// Adiciona clientes na lista
void adicionar_cliente(Cliente* c) 
{
    pthread_mutex_lock(&clientes_lock);
    if (num_clientes < limite_clientes) 
    {
        clientes[num_clientes++] = c;
    }
    pthread_mutex_unlock(&clientes_lock);
}

// Remove clientes da lista
void remover_cliente(Cliente* c) 
{
    pthread_mutex_lock(&clientes_lock);
    for (int i = 0; i < num_clientes; i++) 
    {
        if (clientes[i] == c) 
        {
            // desloca os próximos para trás
            for (int j = i; j < num_clientes-1; j++) 
            {
                clientes[j] = clientes[j+1];
            }
            num_clientes--;
            break;
        }
    }
    pthread_mutex_unlock(&clientes_lock);
}

/*===================================================================
Threads
=====================================================================*/

void* envia_output(void* arg)       // Todos os outputs do server são enviados aqui
{
    char buffer[2048];
    int cliente_id;

    time_t ultima_hora = 0;

    while (1) 
    {
        // Bloqueia para acessar a fila
        pthread_mutex_lock(&fila_lock);

        // Espera enquanto a fila estiver vazia, mas desbloqueia periodicamente
        while (VaziaFila(fila_mensagens)) 
        {
            pthread_mutex_unlock(&fila_lock);

            // Verifica envio da hora
            time_t agora = time(NULL);
            if (agora - ultima_hora >= 60) 
            {
                char horario[16];
                char msg_hora[64];
                struct tm* tm_info = localtime(&agora);
                strftime(horario, sizeof(horario), "[%H:%M]", tm_info);
                snprintf(msg_hora, sizeof(msg_hora), "---- HORA ATUAL: %s ----\n", horario);

                pthread_mutex_lock(&clientes_lock);
                for (int i = 0; i < num_clientes; i++) 
                {
                    send(clientes[i]->socket, msg_hora, strlen(msg_hora), 0);
                }
                pthread_mutex_unlock(&clientes_lock);

                ultima_hora = agora;
            }

            // Pequena pausa para não consumir 100% CPU
            usleep(100000); // 0,1s

            pthread_mutex_lock(&fila_lock);
        }

        // Retira mensagem da fila
        RetiraFila(fila_mensagens, buffer, sizeof(buffer), &cliente_id);
        pthread_mutex_unlock(&fila_lock);

        // Envia mensagem para todos os clientes
        pthread_mutex_lock(&clientes_lock);
        for (int i = 0; i < num_clientes; i++) 
        {
            char msg_final[4096];
            if (clientes[i]->socket == cliente_id)
                snprintf(msg_final, sizeof(msg_final), "[Você enviou]: %s", buffer);
            else
                snprintf(msg_final, sizeof(msg_final), "%s", buffer);

            send(clientes[i]->socket, msg_final, strlen(msg_final), 0);
        }
        pthread_mutex_unlock(&clientes_lock);
    }

    return NULL;
}

void* recebe_mensagens(void* arg)   // Todas as mensagens do client são processadas aqui
{
    Cliente* c = (Cliente*)arg;
    char buffer[1024];
    int n;

    while(1) 
    {
        memset(buffer, 0, sizeof(buffer));
        n = recv(c->socket, buffer, sizeof(buffer) - 1, 0);

        if (n <= 0) 
        {   
            perror("Cliente desconectado");
            break;
        }

        // Horário da mensagem
        time_t t = time(NULL);
        struct tm* tm_info = localtime(&t);
        char msg_completa[2048];
        char horario[16];
        strftime(horario, sizeof(horario), "[%H:%M]", tm_info);
        snprintf(msg_completa, sizeof(msg_completa), "%s %s: %s", c->nome, horario, buffer);

        // Adiciona mensagem na fila
        pthread_mutex_lock(&fila_lock);
        InsereFila(fila_mensagens, msg_completa, c->socket);
        pthread_cond_signal(&fila_cond);
        pthread_mutex_unlock(&fila_lock);
    }

    printf("%s desconectou.\n", c->nome);
    remover_cliente(c);
    close(c->socket);
    free(c);
    return NULL;
}

/*==========================================================================
Main
============================================================================*/

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in endereco;
    int addrlen = sizeof(endereco);

    // Configurando fuso horário
    setenv("TZ", "America/Sao_Paulo", 1);
    tzset();

    pthread_t thread_broadcast;

    // Verificando argumentos
    if (argc < 2) 
    {
        fprintf(stderr, "Uso correto: %s <limite_de_clientes>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    limite_clientes = atoi(argv[1]);
    if (limite_clientes <= 0 || limite_clientes > MAX_CLIENTS) 
    {
        fprintf(stderr, "Limite inválido (1-%d)\n", MAX_CLIENTS);
        exit(EXIT_FAILURE);
    }

    // Inicializando fila de mensagens
    fila_mensagens = CriaFila();
    
    // Criando socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        perror("Falha ao criar o socket");
        exit(EXIT_FAILURE);
    }

    endereco.sin_family = AF_INET;
    endereco.sin_port = htons(PORT);
    endereco.sin_addr.s_addr = INADDR_ANY;
    
    if(bind(sockfd, (struct sockaddr *)&endereco, sizeof(endereco)) < 0)
    {
        perror("Falha no bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, 3) < 0)
    {
        perror("Falha no listen");
        exit(EXIT_FAILURE);
    }

    printf("Servidor iniciado na porta %d Aguardando conexões...\n", PORT);

    pthread_create(&thread_broadcast, NULL, envia_output, NULL);

    // Loop de conexões
    while(1)
    {
        int novo_socket = accept(sockfd, (struct sockaddr *)&endereco, (socklen_t*)&addrlen);
        if(novo_socket < 0)
        {
            perror("Falha no accept");
            continue;
        }

        // Limite de clientes
        pthread_mutex_lock(&clientes_lock);
        if (num_clientes >= limite_clientes) 
        {
            pthread_mutex_unlock(&clientes_lock);
            printf("Conexão recusada: limite de clientes atingido.\n");
            send(novo_socket, "Servidor cheio!\n", 16, 0);
            close(novo_socket);
            continue;
        }
        pthread_mutex_unlock(&clientes_lock);

        Cliente* c = (Cliente*)malloc(sizeof(Cliente));
        c->socket = novo_socket;
        snprintf(c->nome, sizeof(c->nome), "Cliente%d", novo_socket);

        adicionar_cliente(c);

        // Envia hora atual imediatamente
        time_t agora = time(NULL);
        struct tm* tm_info = localtime(&agora);
        char horario[16];
        char msg_hora[64];
        strftime(horario, sizeof(horario), "[%H:%M]", tm_info);
        snprintf(msg_hora, sizeof(msg_hora), "---- HORA ATUAL: %s ----\n", horario);
        send(c->socket, msg_hora, strlen(msg_hora), 0);

        pthread_create(&c->thread_processa, NULL, recebe_mensagens, c);

        printf("Novo cliente conectado: %s\n", c->nome);
    }

    close(sockfd);

    // Fecha todos os clientes conectados
    pthread_mutex_lock(&clientes_lock);
    for (int i = 0; i < num_clientes; i++) 
    {
        pthread_cancel(clientes[i]->thread_processa);
        pthread_join(clientes[i]->thread_processa, NULL);
        close(clientes[i]->socket);
        free(clientes[i]);
    }
    pthread_mutex_unlock(&clientes_lock);

    // Espera thread de broadcast encerrar
    pthread_join(thread_broadcast, NULL);

    LiberaFila(fila_mensagens);

    printf("Servidor finalizado com sucesso.\n");
    return 0;
}