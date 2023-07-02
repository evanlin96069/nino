# nino

[![Build](https://github.com/evanlin96069/nino/actions/workflows/build.yml/badge.svg)](https://github.com/evanlin96069/nino/actions?query=branch%3Amaster)

![screenshot](docs/img/nino_v0.0.3.png)

A small terminal-based text editor written in C.

Inspired by [kilo](https://github.com/antirez/kilo)
and [snaptoken's Build Your Own Text Editor tutorial](https://viewsourcecode.org/snaptoken/kilo/).

## Why?
I'm not used to vim and I don't like nano either, so I make my own text editor.

## Features
- Basic syntax highlight
- Basic UTF-8 support
- Multiple editor tabs
- Automatic indentation and bracket completion
- Mouse support
- Cut, copy, and paste selected section
- Multiple undo/redo
- Search with smart case sensitivity
- File explorer

## Installation
### Linux
nino requires a sufficiently Unix-like C library and a C99 compiler.

Build nino:
```
git clone https://github.com/evanlin96069/nino.git
cd nino
make && sudo make install
```

### Windows
Use Mingw-w64. Run `make` and get `nino.exe` in the `release` folder

## Documentation
- [Configurations](docs/configs.md)
- [Controls](docs/controls.md)
- [Syntax Highlighting](docs/syntax.md)
