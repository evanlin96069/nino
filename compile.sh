#!/bin/sh -e

CC=gcc

warnings="-Wall -Wextra -pedantic -std=c99"
cflags="-Os"

$CC src/*.c -o nino $warngins $cflags
