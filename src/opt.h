#ifndef OPT_H
#define OPT_H

#include <stdio.h>
#include <stdlib.h>

#if defined(__GNUC__) && !defined(__llvm__) && !defined(__INTEL_COMPILER)
#pragma GCC diagnostic ignored "-Wswitch-unreachable"
#endif

#define FOR_OPTS(argc, argv)                                                 \
    for (const char *_p = (--(argc), *++(argv)), *_p1;                       \
         argc && *_p == '-' && *++_p &&                                      \
         (*_p != '-' || _p[1] || (++(argv), --(argc), 0));                   \
         _p = *++(argv), --(argc), (void)(_p1) /* suppress unused warning */ \
    )                                                                        \
        while (*_p)                                                          \
            switch (*_p++)                                                   \
                if (0)                                                       \
                default: {                                                   \
                    fprintf(stderr, "unknown argument: -%c\n", _p[-1]);      \
                    exit(1);                                                 \
                }                                                            \
                    else /* { cases/code here } */

#define OPTARG(argc, argv)                                                    \
    (*_p ? (_p1 = _p, _p = "", _p1)                                           \
         : (*(--(argc), ++(argv))                                             \
                ? *(argv)                                                     \
                : (fprintf(stderr, "argument to '-%c' is missing\n", _p[-1]), \
                   exit(1), (char*)0)))

#endif
