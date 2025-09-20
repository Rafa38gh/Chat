#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h> // sock_addr_in
#include <pthread.h>
#include <time.h>


#define PORT 8080
#define MAX_CLIENTS 10


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

char output[1024];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t clientes_lock = PTHREAD_MUTEX_INITIALIZER;
int nova_mensagem = 0;


// Adiciona clientes na lista
void adicionar_cliente(Cliente* c) {
    pthread_mutex_lock(&clientes_lock);
    if (num_clientes < limite_clientes) {
        clientes[num_clientes++] = c;
    }
    pthread_mutex_unlock(&clientes_lock);
}

// Remove clientes da lista
void remover_cliente(Cliente* c) {
    pthread_mutex_lock(&clientes_lock);
    for (int i = 0; i < num_clientes; i++) {
        if (clientes[i] == c) {
            // desloca os próximos para trás
            for (int j = i; j < num_clientes-1; j++) {
                clientes[j] = clientes[j+1];
            }
            num_clientes--;
            break;
        }
    }
    pthread_mutex_unlock(&clientes_lock);
}

void* envia_output(void* arg)       // Todos os outputs do server são enviados aqui
{
    int sock = *(int*)arg;
    time_t ultima_hora = 0;
    
    while(1)
    {
        pthread_mutex_lock(&lock);
        while(!nova_mensagem)
        {
            time_t agora = time(NULL);
            if(agora - ultima_hora >= 60)
            {
                char horario[32];
                struct tm* tm_info = localtime(&agora);
                strftime(horario, sizeof(horario), "\nHora Atual: %H:%M\n", tm_info);
                send(sock, horario, strlen(horario), 0);
                ultima_hora = agora;
            }

            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            pthread_cond_timedwait(&cond, &lock, &ts);
        }

        if(nova_mensagem)
        {
            send(sock, output, strlen(output), 0);
            nova_mensagem = 0;
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

void* recebe_mensagens(void* arg)   // Todas as mensagens do client são processadas aqui
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
            printf("Mensagem: %s", buffer);
            fflush(stdout);
            
            // Processar mensagem
            pthread_mutex_lock(&lock);
            snprintf(output, sizeof(output), "Servidor recebeu: %.*s", (int)(sizeof(output)-20), buffer);
            nova_mensagem = 1;
            pthread_cond_signal(&cond);
            pthread_mutex_unlock(&lock);

        } else if(n == 0)
        {
            printf("Conexão encerrada pelo cliente.\n");
            break;

        } else
        {
            perror("Erro ao receber dados do cliente.\n");
            break;
        }
    }
    return NULL;
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

    //======================================================================

    // Verificando argumentos
    if (argc < 2) 
    {
        fprintf(stderr, "Uso: %s <limite_de_clientes>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    limite_clientes = atoi(argv[1]);
    if (limite_clientes <= 0 || limite_clientes > MAX_CLIENTS) 
    {
        fprintf(stderr, "Limite inválido (1-%d)\n", MAX_CLIENTS);
        exit(EXIT_FAILURE);
    }
    
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

    printf("Servidor iniciado na porta %d Aguardando conexões...\n", PORT);

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

        // Criando cliente
        Cliente* c = (Cliente*)malloc(sizeof(Cliente));
        c->socket = novo_socket;
        snprintf(c->nome, sizeof(c->nome), "Cliente%d", novo_socket);

        adicionar_cliente(c);

        pthread_create(&c->thread_envia, NULL, envia_output, &c->socket);
        pthread_create(&c->thread_processa, NULL, recebe_mensagens, &c->socket);

        printf("Novo cliente conectado: %s\n", c->nome);
    }

    close(sockfd);
    
    return 0;
}