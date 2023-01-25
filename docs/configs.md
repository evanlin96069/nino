# Configs
Set config using `Ctrl+P` or create the config file `~/.ninorc`.

## Commands and ConVars
| Name | Default | Description |
| - | - | - |
| `tabsize` | 4 | Tab size. |
| `whitespace` | 0 | Use whitespace instead of tab. |
| `autoindent` | 0 | Enable auto indent. |
| `backspace` | 0 | Use hungry backspace. |
| `bracket` | 0 | Use auto bracket completion. |
| `trailing` | 0 | Highlight trailing spaces. |
| `syntax` | 0 | Enable syntax highlight. |
| `helpinfo` | 1 | Show the help information. |
| `ignorecase` | 0 | Use case insensitive search. Set to 2 to use smartcase. |
| `mouse` | 0 | Enable mouse mode. |
| `color` | cmd | Change the color of an element. |
| `help` | cmd | Find help about a convar/concommand. |

## Color
`color <element> [color]`

When color code is `000000` it will be transparent.

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
| `hl.space` | ff6464 |

## Example
An [example](example.ninorc) of `~/.ninorc`.
