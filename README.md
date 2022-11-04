# nino

![screenshot](/img/editor_screenshot.png)

A small terminal-based text editor written in C.

I made this a while ago following [snaptoken's Build Your Own Text Editor tutorial](https://viewsourcecode.org/snaptoken/kilo/)

Also, see the original text editor the tutorial based on: [kilo](https://github.com/antirez/kilo)

## Features
- Basic syntax highlight
- Find
- Line numbers
- Auto indent
- Auto bracket completion
- Select text
- Go to line number
- Use whitespcae
- Mouse support
- Configs
- Cut, copy, and paste

## Configs
Set config using `ctrl+p` or create the config file `~/.ninorc` 

### Example
```
tabsize 2
whitespace 1
autoindent
syntax 1
helpinfo 0
mouse 1
color status.fg f3f3f3
color status.bg 007acc
```
### Options
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

## Actions
| Action | Keybinding |
| - | - |
| Quit | `Ctrl+Q` |
| Save | `Ctrl+S` |
| Find | `Ctrl+F` |
| Go To Line | `Ctrl+G` |
| Configs | `Ctrl+P` |
| Copy | `Ctrl+C` |
| Paste | `Ctrl+V` |
| Cut | `Ctrl+X` |
| Select All | `Ctrl+A` |
| Move Up | `Up` |
| Move Down | `Down` |
| Move Right | `Right` |
| Move Left | `Left` |
| To Line Start | `Home` `Ctrl+Left` |
| To Line End | `End` `Ctrl+Right` |
| To File Start | `Ctrl+Up` `Ctrl+Home` |
| To File End | `Ctrl+Down` `Ctrl+End` |
| To Next Page | `PageUp` |
| To Previous Page | `PageDown` |
| Select Move Up | `Shift+Up` |
| Select Move Down | `Shift+Down` |
| Select Move Right | `Shift+Right` |
| Select Move Left | `Shift+Left` |
| Select To Line Start | `Shift+Home` `Ctrl+Shift+Left` |
| Select To Line End | `Shift+End` `Ctrl+Shift+Right` |

## Install from source
### Linux
```
git clone https://github.com/evanlin96069/nino.git
cd nino
./compile.sh
sudo ./install.sh
```

## Usage
```
nino [filename]
```
