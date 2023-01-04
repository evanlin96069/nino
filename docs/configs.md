# Configs
Set config using `Ctrl+P` or create the config file `~/.ninorc`.

## Commands and ConVars
| Name | Default | Description |
| - | - | - |
| `tabsize` | 4 | Tab size. |
| `whitespace` | 0 | Use whitespace instead of tab. |
| `autoindent` | 0 | Enable auto indent. |
| `syntax` | 0 | Enable syntax highlight. |
| `helpinfo` | 1 | Show the help information. |
| `mouse` | cmd | Toggle. Enable mouse mode. |
| `color` | cmd | Change the color of an element. |
| `help` | cmd | Find help about a convar/concommand. |

## Color
`color <element> [color]`
| Element | Default |
| - | - |
| `bg` | 000000 |
| `top.fg` | e5e5e5 |
| `top.bg` | 1c1c1c |
| `prompt.fg` | e5e5e5
| `prompt.bg` | 000000 |
| `lineno.fg` | 7f7f7f |
| `lineno.bg` | 000000 |
| `status.fg` | e1dbef |
| `status.bg` | 575068 |
| `hl.normal` | e5e5e5 |
| `hl.comment` | 6a9955 |
| `hl.keyword1` | c586c0 |
| `hl.keyword2` | 569cd6 |
| `hl.keyword3` | 4ec9b0 |
| `hl.string` | ce9178 |
| `hl.number` | b5cea8 |
| `hl.match` | 592e14 |
| `hl.select` | 264f78 |

## Example
An example of `~/.ninorc`.
```
tabsize 2
whitespace 1
autoindent
syntax 1
helpinfo 0
mouse
color status.fg f3f3f3
color status.bg 007acc
```
