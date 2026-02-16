#include "bpe.h"
#include <stddef.h>
#include <stdint.h>

bool dump_tokens(const char *file_path, Tokens tokens){
    return write_entire_file(file_path, tokens.items, tokens.count * sizeof(*tokens.items));
}

bool dump_pairs(const char *file_path, Pairs pairs) {
    bool result = true;
    String_Builder sb = {0};

    // sb_append_cstr(&sb, "BPE");
    // sb_append(&sb, BPE_VERSION_CURRENT);
    sb_append_buf(&sb, pairs.items, pairs.count*sizeof(*pairs.items));
    if(!write_entire_file(file_path, sb.items, sb.count)) return_defer(false);

defer:
    free(sb.items);
    return result;
}


bool load_tokens(const char *file_path, Tokens *tokens, String_Builder *tmp_sb){
    tmp_sb->count = 0;
    if(!read_entire_file(file_path, tmp_sb)) return false;
    if(tmp_sb->count%sizeof(*tokens->items) != 0) {
	nob_log(ERROR, "%s: file size in bytes (%zu) must be divisible by %zu", file_path, tmp_sb->count, sizeof(*tokens->items));
	return false;
    }

    uint32_t *items = (void*)tmp_sb->items;
    size_t items_count = tmp_sb->count/sizeof(*tokens->items);
    for(size_t i = 0; i < items_count; ++i) {
	da_append(tokens, items[i]);
    }
    return true;
}

bool parse_pairs(const char *file_path, Pairs *pairs, String_View data) {
    if(data.count%sizeof(*pairs->items) != 0) {
	nob_log(ERROR, "%s: file size in bytes (%zu) must be divisible by %zu", file_path, data.count, sizeof(*pairs->items));
	return false;
    }

    Pair *items = (void *)data.data;
    size_t items_count = data.count / sizeof(*pairs->items);

    if (items_count < BPE_PRELUDE_SIZE) {
	nob_log(ERROR, "%s: pair count %zu is too small. It must be at least %d",file_path, items_count, BPE_PRELUDE_SIZE);
	return false;
    }


    for (uint32_t i = 0; i < BPE_PRELUDE_SIZE; ++i) {
	if (items[i].l != i) {
            nob_log(ERROR, "%s: pair count %zu is too small. It must be at least %d",file_path, items_count, BPE_PRELUDE_SIZE);
	    return false;
        }

        if (items[i].r != 0) {
            nob_log(ERROR, "%s pair %u: Right subtoken is equal to %u instead of 0", file_path, i, items[i].r);
            return false;
        }

        da_append(pairs, items[i]);
    }

    for (uint32_t i = BPE_PRELUDE_SIZE; i < items_count; ++i) {
	if (items[i].l >= items_count) {
            nob_log(ERROR, "%s: pair %u: Left subtoken is %u >= %zu", file_path, i, items[i].l, items_count);
            return false;
        }
        if (items[i].r >= items_count) {
            nob_log(ERROR, "%s: pair %u: Right subtoken is %u >= %zu", file_path, i, items[i].r, items_count);
        }

        da_append(pairs, items[i]);
    }
    return true;    
}

bool load_pairs(const char *file_path, Pairs *pairs, String_Builder *tmp_sb) {
    tmp_sb->count = 0;
    if(!read_entire_file(file_path, tmp_sb)) return false;

    String_View data = sb_to_sv(*tmp_sb);

    if(!parse_pairs(file_path, pairs, data)) return false;
    return true;
}


void render_token(Pairs pairs, uint32_t token, Nob_String_Builder *sb)
{
    assert(token < pairs.count);

    if(token == pairs.items[token].l){
        da_append(sb,(char)token);
        return;
    }

    render_token(pairs, pairs.items[token].l,sb);
    render_token(pairs, pairs.items[token].r,sb);
}

void c_strlit_escape_bytes(const char *bytes, size_t bytes_size, Nob_String_Builder *sb_out) {
    for(size_t i = 0; i < bytes_size; ++i) {
        if(bytes[i] == '"') {
            sb_append_cstr(sb_out, "\\\"");
        } else if(bytes[i] == "\\") {
            sb_append_cstr(sb_out, "\\\\");
        } else if(isprint(bytes[i])) {
            da_append(sb_out, bytes[i]);
        } else {
            sb_appendf(sb_out, "\\x%02X", (uint8_t)bytes[i]);
        }
    }
}

#define NOB_IMPLEMENTATION
#include "nob.h"



