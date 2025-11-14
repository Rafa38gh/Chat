#ifndef EMOJI_TABLE_H
#define EMOJI_TABLE_H

#include <stdbool.h>

bool emoji_table_load(const char *filename);
const char *emoji_get(const char *category, const char *name);
void emoji_table_free();

#endif
