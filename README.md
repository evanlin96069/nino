# nino

[![Build](https://github.com/evanlin96069/nino/actions/workflows/build.yml/badge.svg)](https://github.com/evanlin96069/nino/actions?query=branch%3Amaster)

![screenshot](docs/img/nino_v0.0.3.png)

A small terminal-based text editor written in C.

Inspired by [kilo](https://github.com/antirez/kilo)
and [snaptoken's Build Your Own Text Editor tutorial](https://viewsourcecode.org/snaptoken/kilo/).

## Why?
I'm not used to [Vim](https://www.vim.org/) and I don't like [nano](https://nano-editor.org/) either, so I make my own text editor. (I don't know about [micro](https://micro-editor.github.io/) back then...)

## Features
- Basic syntax highlight
- Basic UTF-8 support
- Multiple editor tabs
- Automatic indentation and bracket completion
- Mouse support
- Cut, copy, and paste the selected section
- Multiple undo/redo
- Search with smart case sensitivity
- File explorer

## Build

This project uses CMake for building. Follow these steps to build the project:

### Prerequisites

- CMake (minimum required version, e.g., 3.15)
- A suitable C compiler (GCC, Clang, MSVC, etc.)

### Cloning the Repository

```bash
git clone https://github.com/evanlin96069/nino.git
cd nino
```

### Configuring the Build

Create a build directory and run CMake to configure the project:

```bash
mkdir build
cd build
cmake ..
```

### Building the Project

To build the project, run:

```bash
cmake --build .
```

### Optional: Installing the Project

If installation is necessary, you can install the project using:

```bash
cmake --install .
```

## Documentation
- [Configurations](docs/configs.md)
- [Controls](docs/controls.md)
- [Syntax Highlighting](docs/syntax.md)
