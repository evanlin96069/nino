# nino
A small terminal-based text editor written in C.

I made this a while ago following [snaptoken's Build Your Own Text Editor tutorial](https://viewsourcecode.org/snaptoken/kilo/)

Also, see the original text editor the tutorial based on: [kilo](https://github.com/antirez/kilo)

## Features
- Basic syntax highlight
- Find
- Line numbers
- Auto indent
- Auto bracket completion
- Selected text
- Go to line number
- Use whitespcae
- Mouse mode
- Configs

## Configs
Set config using `ctrl+p` or create the config file `~/.ninorc` 

Example:
```
tabsize 2
whitespace 1
color status.bg 0078d7
```
- `tabsize` [size]
- `whitespace` [0|1]
- `autoindent` [0|1]
- `syntax` [0|1]
- `helpinfo` [0|1]
- `mouse` [0|1]
- `color` [target] [color]
    - `status.fg`
    - `status.bg`
    - `hl.normal`
    - `hl.comment`
    - `hl.keyword1`
    - `hl.keyword2`
    - `hl.keyword3`
    - `hl.string`
    - `hl.number`
    - `hl.match`
    - `hl.select`

## Install from source
```
git clone https://github.com/evanlin96069/nino.git
cd nino
./compile.sh
sudo ./install.sh
```

