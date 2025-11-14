#ifndef FILA_H_INCLUDED
#define FILA_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAM_MSG 1024

typedef struct nos
{
    char mensagem[TAM_MSG];
    int cliente_id;
    struct nos* prox;
}Nos;

typedef struct fila
{
    Nos* ini;
    Nos* fim;
} Fila;

int VaziaFila(Fila* f) {
    int vazia;
    vazia = (f->ini == NULL);
    return vazia;
}

Fila* CriaFila()
{
    Fila* f = (Fila*) malloc(sizeof(Fila));
    f->ini = f->fim = NULL;
    return f;
}

void InsereFila(Fila* f, const char* msg, int cliente_id) {
    Nos* n = (Nos*) malloc(sizeof(Nos));
    strncpy(n->mensagem, msg, TAM_MSG - 1);
    n->mensagem[TAM_MSG - 1] = '\0';
    n->cliente_id = cliente_id;
    n->prox = NULL;

    if (f->fim) {
        f->fim->prox = n;
    }
    f->fim = n;
    if (!f->ini) {
        f->ini = n;
    }
}

void RetiraFila(Fila* f, char* buffer, size_t tamanho, int* cliente_id) 
{
    if (VaziaFila(f)) 
    {
        buffer[0] = '\0';
        if (cliente_id) *cliente_id = -1;
        return;
    }
    Nos* n = f->ini;
    strncpy(buffer, n->mensagem, tamanho - 1);
    buffer[tamanho - 1] = '\0';

    if (cliente_id) *cliente_id = n->cliente_id;

    f->ini = n->prox;
    if (!f->ini) {
        f->fim = NULL;
    }
    free(n);
}

void ImprimeFila(Fila* f) {
    Nos* p = f->ini;
    printf("\nFila:\n");
    while (p) {
        printf(" %s\n", p->mensagem);
        p = p->prox;
    }
}

void LiberaFila(Fila* f) {
    Nos* p = f->ini;
    while (p) {
        Nos* t = p->prox;
        free(p);
        p = t;
    }
    free(f);
}


#endif // FILA_H_INCLUDED
