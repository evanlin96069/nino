#include "opt.h"

OptParser optInit(const VecStr* args) {
    return (OptParser){
        .args = args,
        .index = 1,
        .offset = 0,
    };
}

int optNext(OptParser* parser) {
    size_t index = parser->index;
    size_t offset = parser->offset;
    const VecStr* args = parser->args;
    if (index >= args->size)
        return 0;

    StrView arg = svFromStr(args->data[index]);
    if (offset >= arg.size)
        return 0;
    if (offset == 0) {
        if (arg.data[0] != '-')
            return 0;
        offset++;
        if (offset >= arg.size)  // "-"
            return 0;
        if (arg.size == 2 && arg.data[1] == '-') {
            // "--"
            parser->index++;
            parser->offset = 0;
            return 0;
        }
    }

    int flag = arg.data[offset];
    offset++;
    if (offset >= arg.size) {
        index++;
        offset = 0;
    }
    parser->index = index;
    parser->offset = offset;
    return flag;
}

const char* optArg(OptParser* parser) {
    size_t index = parser->index;
    size_t offset = parser->offset;
    const VecStr* args = parser->args;
    if (index >= args->size)
        return NULL;

    StrView arg = svFromStr(args->data[index]);
    if (offset >= arg.size) {
        return NULL;
    }

    const char* result = &arg.data[offset];
    parser->index++;
    parser->offset = 0;
    return result;
}

const Str* optRemaining(const OptParser* parser, size_t* count) {
    size_t index = parser->index;
    size_t offset = parser->offset;
    const VecStr* args = parser->args;

    if (offset != 0)
        index++;

    if (index >= args->size) {
        *count = 0;
        return NULL;
    }

    *count = args->size - index;
    return &args->data[index];
}
