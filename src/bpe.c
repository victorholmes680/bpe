#include "bpe.h"

bool dump_tokens(const char *file_path, Tokens tokens){
    return write_entire_file(file_path, tokens.items, tokens.count * sizeof(*tokens.items));
}

bool dump_pairs(const char *file_path, Pairs pairs) {
    bool result = false;
    String_Builder sb = {0};

    sb_append_cstr(&sb, "BPE");
    sb_append(&sb, BPE_VERSION_CURRENT);
    sb_append_buf(&sb, pairs.items, pairs.count*sizeof(*pairs.items));
    if(!write_entire_file(file_path, sb.items, sb.count)) return_defer(false);

defer:
    free(sb.items);
    return result;
}


#define NOB_IMPLEMENTATION
#include "nob.h"



