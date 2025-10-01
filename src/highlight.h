#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include "utils.h"

typedef struct EditorFile EditorFile;
typedef struct EditorRow EditorRow;

#define HL_FG_MASK 0x0F
#define HL_BG_MASK 0xF0
#define HL_FG_BITS 4

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

enum EditorHighlightFg {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_KEYWORD3,
    HL_STRING,
    HL_NUMBER,
    HL_SPACE,

    HL_FG_COUNT,
};

enum EditorHighlightBg {
    HL_BG_NORMAL = 0,
    HL_BG_MATCH,
    HL_BG_SELECT,
    HL_BG_TRAILING,

    HL_BG_COUNT,
};

typedef struct EditorSyntax {
    struct EditorSyntax* next;

    const char* file_type;
    const char* singleline_comment_start;
    const char* multiline_comment_start;
    const char* multiline_comment_end;
    VECTOR(const char*) file_exts;
    VECTOR(const char*) keywords[3];
    int flags;

    struct JsonValue* value;
} EditorSyntax;

void editorUpdateSyntax(EditorFile* file, EditorRow* row);
void editorSetSyntaxHighlight(EditorFile* file, EditorSyntax* syntax);
void editorSelectSyntaxHighlight(EditorFile* file);
void editorInitHLDB(void);
bool editorLoadHLDB(const char* json_file);
void editorFreeHLDB(void);

#endif
