#ifndef OPT_H
#define OPT_H

#include "utils/utils.h"

typedef struct OptParser {
    const VecStr* args;
    size_t index;
    size_t offset;
} OptParser;

OptParser optInit(const VecStr* args);
int optNext(OptParser* parser);
const char* optArg(OptParser* parser);
const Str* optRemaining(const OptParser* parser, size_t* count);

#endif
