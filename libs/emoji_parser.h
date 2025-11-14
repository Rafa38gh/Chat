#ifndef EMOJI_PARSER_H
#define EMOJI_PARSER_H

// Retorna uma NOVA string processada (malloc).
// O chamador DEVE dar free().
char *emoji_parse_message(const char *msg);

#endif
