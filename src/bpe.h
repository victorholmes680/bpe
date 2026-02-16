#ifndef BPE_H_
#define BPE_H_


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define NOB_STRIP_PREFIX
#include "nob.h"

#undef rename

#define BPE_PRELUDE_SIZE 256

#define BPE_VERSION_CURRENT 1

// dynamic array Tokens
typedef struct {
    uint32_t *items;
    size_t count;
    size_t capacity;
} Tokens;

// single Pair structure
typedef struct {
    uint32_t l, r;
    uint64_t freq;
} Pair;

// dynamic array Pairs
typedef struct {
    Pair *items;
    size_t count;
    size_t capacity;
} Pairs;

bool dump_pairs(const char *file_path, Pairs pairs);
bool dump_tokens(const char *file_path, Tokens tokens);
bool load_tokens(const char *file_path, Tokens *tokens, String_Builder *tmp_sb);
bool load_pairs(const char *file_path, Pairs *pairs, String_Builder *tmp_sb);
void render_token(Pairs pairs, uint32_t token, String_Builder *sb);
void c_strlit_escape_bytes(const char *bytes, size_t bytes_size, String_Builder *sb_out);
#endif
