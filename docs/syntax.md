# Syntax Highlighting
nino supports simple keywords syntax highlighting.

Enable syntax highlighting with command `syntax 1`.

## Install Syntax Highlighting Data
Copy the `syntax` folder to the [configuration directory](configs.md).

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
