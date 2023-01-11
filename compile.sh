#!/bin/sh -e

CC=gcc

warnings="-Wall -Wextra"

dbg=0
if [ $dbg = 1 ]; then
    cflags="-g3"
else
    cflags="-Os -s"
fi

$CC src/*.c -pedantic -std=c11 $warnings $cflags -o nino
