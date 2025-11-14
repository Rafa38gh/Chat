#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h> // sock_addr_in
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "libs/fila.h"
#include "libs/emoji_table.h"
#include "libs/emoji_parser.h"

#define PORT 8080
#define MAX_CLIENTS 10

// Cores ANSI:
#define RESET   "\033[0m"
#define RED     "\033[1;31m"
#define YELLOW  "\033[1;33m"
#define GREEN   "\033[1;32m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"

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
                snprintf(msg_hora, sizeof(msg_hora),YELLOW"---- HORA ATUAL: %s ----\n"RESET, horario);

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
                snprintf(msg_final, sizeof(msg_final), YELLOW"[Você enviou]:"RESET"%s\n", buffer);
            else
                snprintf(msg_final, sizeof(msg_final), "%s\n", buffer);

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

        buffer[strcspn(buffer, "\r\n")] = 0;

        // ===============================================================
        // 1) DETECTAR SE É COMANDO OU NÃO (melhor: evita colisão com emojis)
        // ===============================================================
        size_t len = strlen(buffer);
        bool eh_comando = false;

        if (len > 0 && buffer[0] == ':')
        {
            // procura o primeiro ':' de fechamento depois do inicial
            char *end_colon = strchr(buffer + 1, ':');

            // procura o primeiro espaço
            char *space = strchr(buffer, ' ');

            // Se existe um :fechamento: antes do primeiro espaço (ou não há espaço),
            // então é um token :emoji: no começo => NÃO é comando.
            if (end_colon != NULL && (space == NULL || end_colon < space))
            {
                eh_comando = false; // emoji no começo — tratar como mensagem
            }
            else
            {
                // Caso não haja um :fechamento: antes do espaço, pode ser comando.
                // Verifica comandos conhecidos
                if (strncmp(buffer, ":nome ", 6) == 0 || strcmp(buffer, ":quit") == 0)
                {
                    eh_comando = true;
                }
                else
                {
                    // Se começa com ':' e não bate com comandos conhecidos,
                    // consideramos comando inválido (mesma lógica antiga).
                    eh_comando = true;
                }
            }
        }

        // ===============================================================
        // 2) PROCESSAMENTO DE COMANDOS
        // ===============================================================
        if (eh_comando)
        {
            if (strncmp(buffer, ":nome ", 6) == 0)                                          // :nome
            {
                if (strlen(buffer + 6) == 0)
                {
                    char erro[64] = RED"Uso correto :nome <novo_nome>\n"RESET;
                    send(c->socket, erro, strlen(erro), 0);
                }
                else
                {
                    pthread_mutex_lock(&clientes_lock);
                    snprintf(c->nome, sizeof(c->nome), "%s", buffer + 6);
                    pthread_mutex_unlock(&clientes_lock);

                    char msg_confirm[128];
                    snprintf(msg_confirm, sizeof(msg_confirm),
                             YELLOW"Seu nome foi alterado para: "RESET GREEN"%s\n"RESET,
                             c->nome);
                    send(c->socket, msg_confirm, strlen(msg_confirm), 0);
                }
                continue;
            }
            else if (strcmp(buffer, ":quit") == 0)                                          // :quit
            {
                char quit[64] = YELLOW"Você saiu do servidor.\n"RESET;
                send(c->socket, quit, strlen(quit), 0);
                break;
            }
            else if (strncmp(buffer, ":pm ", 4) == 0)                                       // :pm <nome> <mensagem>
            {
                char *args = buffer + 4;

                // Primeiro token: nome
                char *nome_dest = strtok(args, " ");
                char *mensagem  = strtok(NULL, "");

                if (!nome_dest || !mensagem)
                {
                    char erro[128] = RED"Uso correto: :pm <nome> <mensagem>\n"RESET;
                    send(c->socket, erro, strlen(erro), 0);
                    continue;
                }

                // Procurar cliente no vetor global
                pthread_mutex_lock(&clientes_lock);

                Cliente *dest = NULL;
                for (int i = 0; i < num_clientes; i++)
                {
                    if (clientes[i] && strcmp(clientes[i]->nome, nome_dest) == 0)
                    {
                        dest = clientes[i];
                        break;
                    }
                }

                pthread_mutex_unlock(&clientes_lock);

                if (!dest)
                {
                    char erro[128];
                    snprintf(erro, sizeof(erro),
                            RED"Usuário '%s' não encontrado.\n"RESET,
                            nome_dest);
                    send(c->socket, erro, strlen(erro), 0);
                    continue;
                }

                // ---- PROCESSAR EMOJIS ----
                char *msg_processada = emoji_parse_message(mensagem);
                if (!msg_processada)
                {
                    msg_processada = strdup(mensagem);
                }
                                
                // Enviar para o destinatário
                char msg_dest[2048];
                snprintf(msg_dest, sizeof(msg_dest),
                        MAGENTA"[PM de %s]: "RESET"%s\n",
                        c->nome, msg_processada);

                send(dest->socket, msg_dest, strlen(msg_dest), 0);

                // Confirmação para o remetente
                char msg_self[2048];
                snprintf(msg_self, sizeof(msg_self),
                        CYAN"[PM para %s]: "RESET"%s\n",
                        nome_dest, msg_processada);

                send(c->socket, msg_self, strlen(msg_self), 0);

                continue;
            }
            else                                                                            // COMANDO INVÁLIDO
            {
                char erro[64] = RED"Comando inválido.\n"RESET;
                send(c->socket, erro, strlen(erro), 0);
                continue;
            }
        }

        // ===============================================================
        // 3) NÃO É COMANDO → PROCESSAR EMOJIS
        // ===============================================================
        char *msg_processada = emoji_parse_message(buffer);
        if (!msg_processada) // fallback seguro
        {
            msg_processada = strdup(buffer);
            if (!msg_processada) msg_processada = (char*)""; // caso extremo
        }

        // ===============================================================
        // 4) MONTAR A MENSAGEM FINAL COM NOME + HORÁRIO
        // ===============================================================
        time_t t = time(NULL);
        struct tm* tm_info = localtime(&t);

        char msg_completa[2048];
        char horario[16];
        strftime(horario, sizeof(horario), "[%H:%M]", tm_info);

        snprintf(msg_completa, sizeof(msg_completa),
                 GREEN"%s"RESET YELLOW"%s"RESET": %s\n",
                 c->nome, horario, msg_processada);

        // ===============================================================
        // 5) INSERIR NA FILA PARA ENVIAR A TODOS
        // ===============================================================
        pthread_mutex_lock(&fila_lock);
        InsereFila(fila_mensagens, msg_completa, c->socket);
        pthread_cond_signal(&fila_cond);
        pthread_mutex_unlock(&fila_lock);

        free(msg_processada);
    }

    printf(GREEN"%s "RESET YELLOW"desconectou.\n"RESET, c->nome);
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
        fprintf(stderr, RED"Uso correto: %s <limite_de_clientes>\n"RESET, argv[0]);
        exit(EXIT_FAILURE);
    }

    limite_clientes = atoi(argv[1]);
    if (limite_clientes <= 0 || limite_clientes > MAX_CLIENTS)
    {
        fprintf(stderr, RED"Limite inválido (1-%d)\n"RESET, MAX_CLIENTS);
        exit(EXIT_FAILURE);
    }

    // Carregando tabela de emojis
    if(!emoji_table_load("emojis.json"))
    {
        fprintf(stderr, RED"Falha ao carregar tabela de emojis.\n"RESET);
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

    printf(YELLOW"Servidor iniciado na porta %d Aguardando conexões...\n"RESET, PORT);

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
            printf(YELLOW"Conexão recusada: limite de clientes atingido.\n"RESET);
            send(novo_socket, "Servidor cheio!\n", 16, 0);
            close(novo_socket);
            continue;
        }
        pthread_mutex_unlock(&clientes_lock);

        Cliente* c = (Cliente*)malloc(sizeof(Cliente));
        c->socket = novo_socket;
        snprintf(c->nome, sizeof(c->nome), "%s:%d", inet_ntoa(endereco.sin_addr), ntohs(endereco.sin_port));

        adicionar_cliente(c);

        // Envia hora atual imediatamente
        time_t agora = time(NULL);
        struct tm* tm_info = localtime(&agora);
        char horario[16];
        char msg_hora[64];
        strftime(horario, sizeof(horario), "[%H:%M]", tm_info);
        snprintf(msg_hora, sizeof(msg_hora), YELLOW"---- HORA ATUAL: %s ----\n"RESET, horario);
        send(c->socket, msg_hora, strlen(msg_hora), 0);

        pthread_create(&c->thread_processa, NULL, recebe_mensagens, c);

        printf(YELLOW"Novo cliente conectado:"RESET GREEN"%s\n"RESET, c->nome);
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
    emoji_table_free();     // Libera tabela de emojis

    LiberaFila(fila_mensagens);

    printf(YELLOW"Servidor finalizado com sucesso.\n"RESET);
    return 0;
}
