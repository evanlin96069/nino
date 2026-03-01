# Configurations
Configurations are set by running commands. Use `Ctrl+P` to open the command prompt.

To run commands on start, create `ninorc` in the configuration directory.

## Configuration Directory:
- Linux: `~/.config/nino`
- Windows: `~/.nino`

## Execute a Config File
`exec` command executes a config file.
The command will first search for the file in the current directory, then the configuration directory.

## Commands and ConVars
| Name | Default | Description |
| - | - | - |
| `tabsize` | 4 | Tab size. |
| `whitespace` | 1 | Use whitespace instead of tab. |
| `autoindent` | 0 | Enable auto indent. |
| `backspace` | 1 | Use hungry backspace. |
| `bracket` | 0 | Use auto bracket completion. |
| `trailing` | 1 | Highlight trailing spaces. |
| `drawspace` | 1 | Render whitespace and tab. |
| `syntax` | 1 | Enable syntax highlight. |
| `helpinfo` | 1 | Show the help information. |
| `ignorecase` | 2 | Use case insensitive search. Set to 2 to use smart case. |
| `mouse` | 1 | Enable mouse mode. |
| `ex_default_width` | 40 | File explorer default width. |
| `ex_show_hidden` | 1 | Show hidden files in the file explorer. |
| `osc52_copy` | 1 | Copy to system clipboard using OSC52. |
| `newline_default` | 0 | Set the default EOL sequence (LF/CRLF). 0 is OS default. |
| `ttimeoutlen` | 50 | Time in milliseconds to wait for a key code sequence to complete. |
| `lineno` | 1 | Show line numbers. |
| `readonly` | 0 | Open files in read-only mode. |
| `shell` | "" | Shell used by the run command. (full path) |
| `color` | cmd | Change the color of an element. |
| `exec` | cmd | Execute a config file. |
| `lang` | cmd | Set the syntax highlighting language of the current file. |
| `hldb_load` | cmd | Load a syntax highlighting JSON file. |
| `hldb_reload_all` | cmd | Reload syntax highlighting database. |
| `newline` | cmd | Set the EOL sequence (LF/CRLF). |
| `unlock` | cmd | Allow editing a read-only file. |
| `reload` | cmd | Reload the current file from disk. |
| `alias` | cmd | Alias a command. |
| `unalias` | cmd | Remove an alias. |
| `cmd_expand_depth` | 1024 | Max depth for alias expansion. |
| `echo` | cmd | Echo text to console. |
| `clear` | cmd | Clear all console output. |
| `help` | cmd | Find help about a convar/concommand. |
| `find` | cmd | Find concommands with the specified string in their name/help text. |
| `version` | cmd | Print version info string. |
| `suspend` | cmd | Suspend the editor (Not available on Windows). |
| `run` | cmd | Run a shell command. |

## Color
`color <element> [color]`

When color code is `000000` it will be transparent.

### Default Theme
| Element | Default |
| - | - |
| `bg` | `1e1e1e` |
| `top.fg` | `e5e5e5` |
| `top.bg` | `252525` |
| `top.tabs.fg` | `969696` |
| `top.tabs.bg` | `2d2d2d` |
| `top.select.fg` | `e5e5e5` |
| `top.select.bg` | `575068` |
| `explorer.bg` | `252525` |
| `explorer.select` | `575068` |
| `explorer.directory` | `ecc184` |
| `explorer.file` | `e5e5e5` |
| `explorer.focus` | `2d2d2d` |
| `prompt.fg` | `e5e5e5` |
| `prompt.bg` | `3c3c3c` |
| `status.fg` | `e1dbef` |
| `status.bg` | `575068` |
| `status.lang.fg` | `e1dbef` |
| `status.lang.bg` | `a96b21` |
| `status.pos.fg` | `e1dbef` |
| `status.pos.bg` | `d98a2b` |
| `lineno.fg` | `7f7f7f` |
| `lineno.bg` | `1e1e1e` |
| `cursorline` | `282828` |
| `hl.normal` | `e5e5e5` |
| `hl.comment` | `6a9955` |
| `hl.keyword1` | `c586c0` |
| `hl.keyword2` | `569cd6` |
| `hl.keyword3` | `4ec9b0` |
| `hl.string` | `ce9178` |
| `hl.number` | `b5cea8` |
| `hl.space` | `3f3f3f` |
| `hl.match` | `592e14` |
| `hl.select` | `264f78` |
| `hl.trailing` | `ff6464` |

## Example
An [example](example.ninorc) of `ninorc`.
