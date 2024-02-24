# Syntax Highlighting
nino supports simple keywords syntax highlighting.

Enable syntax highlighting with command `syntax 1`.

## Add Syntax Highlighting Data
If you would like to make your own syntax files,
you can put them in:
- Linux: `~/.config/nino/syntax`
- Windows: `~/.nino/syntax`

## Syntax Highlighting Data
Syntax highlighting data are stored in JSON files.

```JSON
{
    "name" : "Example language",
    "extensions" : [
        ".extension1",
        ".extension2"
    ],
    "comment": "//",
    "multiline-comment": [
        "/*",
        "*/"
    ],
    "keywords1": [
        "for",
        "while",
        "if",
        "else"
    ],
    "keywords2": [
        "int",
        "char",
        "float"
    ],
    "keywords3": [
        "string"
    ]
}
```
