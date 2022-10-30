#!/bin/sh -e

CC=gcc

warnings="-Wall -Wextra -pedantic -std=c99"

dbg=0
if [ $dbg = 1 ]; then
    cflags="-g3"
else
    cflags="-Os -s"
fi

$CC src/*.c -o nino $warngins $cflags
