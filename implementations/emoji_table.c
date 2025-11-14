#include "../libs/emoji_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libs/cJSON.h"

static cJSON *root = NULL;

bool emoji_table_load(const char *filename) 
{
    FILE *f = fopen(filename, "rb");
    if (!f) 
    {
        printf("Erro: não foi possível abrir %s\n", filename);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc(size + 1);
    if (!buffer) 
    {
        fclose(f);
        printf("Erro: sem memória ao carregar JSON\n");
        return false;
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';

    fclose(f);

    root = cJSON_Parse(buffer);
    free(buffer);

    if (!root) 
    {
        printf("Erro ao parsear JSON de emojis\n");
        return false;
    }

    return true;
}

const char *emoji_get(const char *category, const char *name) 
{
    if (!root) return NULL;

    cJSON *cat = cJSON_GetObjectItem(root, category);
    if (!cat) return NULL;

    cJSON *emoji = cJSON_GetObjectItem(cat, name);
    if (!emoji) return NULL;

    return emoji->valuestring;
}

void emoji_table_free() 
{
    if (root) 
    {
        cJSON_Delete(root);
        root = NULL;
    }
}