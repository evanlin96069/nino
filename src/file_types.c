#include "file_types.h"

#include "defines.h"

static const char* C_HL_extentions[] = {".c", ".h", NULL};

static const char* C_HL_keywords[] = {
    "break",         "case",      "continue",  "default",    "do",
    "else",          "for",       "goto",      "if",         "return",
    "switch",        "while",     "#include",  "#define",    "#undef",
    "#if",           "#ifdef",    "#ifndef",   "#error",     "#pragma",

    "auto|",         "char|",     "const|",    "double|",    "enum|",
    "extern|",       "float|",    "inline|",   "int|",       "long|",
    "register|",     "restrict|", "short|",    "signed|",    "sizeof|",
    "static|",       "struct|",   "typedef|",  "union|",     "unsigned|",
    "void|",         "volatile|", "bool|",     "true|",      "false|",
    "NULL|",         "__FILE__",  "__LINE__",  "__DATE__",   "__TIME__",
    "__TIMESTAMP__",

    "int8_t^",       "int16_t^",  "int32_t^",  "int64_t^",   "uint8_t^",
    "uint16_t^",     "uint32_t^", "uint64_t^", "ptrdiff_t^", "size_t^",
    "wchar_t^",      NULL};

static const char* PYTHON_HL_extentions[] = {".py", NULL};

static const char* PYTHON_HL_keywords[] = {"as",
                                           "assert",
                                           "break",
                                           "continue",
                                           "del",
                                           "elif",
                                           "else",
                                           "except",
                                           "finally",
                                           "for",
                                           "from",
                                           "if",
                                           "import",
                                           "pass",
                                           "raise",
                                           "return",
                                           "try",
                                           "while",
                                           "with",
                                           "yield"

                                           "and|",
                                           "class|",
                                           "def|",
                                           "False|",
                                           "global|",
                                           "in|",
                                           "is|",
                                           "lambda|",
                                           "None|",
                                           "nonlocal|",
                                           "not|",
                                           "or|",
                                           "True|",

                                           "bool^",
                                           "int^",
                                           "float^",
                                           "complex^",
                                           "list^",
                                           "tuple^",
                                           "str^",
                                           "bytes^",
                                           "bytearray^",
                                           "frozenset^",
                                           "set^",
                                           "dict^",
                                           "type^",
                                           NULL};

const EditorSyntax HLDB[] = {
    {"C", C_HL_extentions, C_HL_keywords, "//", "/*", "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},
    {"Python", PYTHON_HL_extentions, PYTHON_HL_keywords, "#", "'''", "'''",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS}};