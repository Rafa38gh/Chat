#include <stdio.h>
#include <stdbool.h>
#include "../libs/emoji_table.h"

int main() {
    if (!emoji_table_load("emojis.json")) {
        printf("Falha ao carregar tabela.\n");
        return 1;
    }

    const char *e = emoji_get("faces", "smile");
    if (!e) {
        printf("Emoji não encontrado.\n");
    } else {
        printf("Emoji encontrado: %s\n", e);
    }

    emoji_table_free();
    return 0;
}
