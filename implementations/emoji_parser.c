#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libs/emoji_table.h"
#include "../libs/emoji_table.h"

// ===============================
// PROCURAR EM QUALQUER CATEGORIA
// ===============================
static const char *emoji_find_any_category(const char *name)
{
    // Lista de categorias do seu emojis.json
    const char *cats[] = {"faces", "hands", "animals", "misc"};
    int total = sizeof(cats) / sizeof(cats[0]);

    for (int i = 0; i < total; i++) {
        const char *e = emoji_get(cats[i], name);
        if (e) return e;
    }

    return NULL; // não encontrou
}

// ===============================
// PARSER PRINCIPAL
// ===============================
//
// Converte tokens no formato :emoji: em emojis reais.
//
// Exemplo:
//   "Oi :smile: e :fire:" → "Oi 😄 e 🔥"
//
char *emoji_parse_message(const char *msg)
{
    // Cria buffer para resultado final
    // (pode aumentar se suas mensagens forem maiores)
    char *result = malloc(4096);
    if (!result) return NULL;

    result[0] = '\0';

    const char *p = msg;

    while (*p) {

        // Verifica se começa com ':'
        if (*p == ':') 
        {
            const char *end = strchr(p + 1, ':');

            if (end) 
            {
                int len = end - (p + 1);

                // Nome do emoji é pequeno e válido?
                if (len > 0 && len < 100) 
                {
                    char name[128];
                    strncpy(name, p + 1, len);
                    name[len] = '\0';

                    const char *emoji = emoji_find_any_category(name);

                    if (emoji) 
                    {
                        // Achou emoji → concatenar ao resultado
                        strcat(result, emoji);

                        // Avança depois do segundo ':'
                        p = end + 1;
                        continue;
                    }
                }
            }
        }

        // Caso contrário → copia caractere normal
        int rlen = strlen(result);
        result[rlen] = *p;
        result[rlen + 1] = '\0';

        p++;
    }

    return result;
}