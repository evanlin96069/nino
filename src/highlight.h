#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include "utils.h"

typedef struct EditorFile EditorFile;
typedef struct EditorRow EditorRow;

// Highlighting flags
#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

typedef enum EditorHLType {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_KEYWORD3,
    HL_STRING,
    HL_NUMBER,
} EditorHLType;

typedef struct EditorHLSpan {
    uint32_t start;
    uint32_t len;
    EditorHLType type;
} EditorHLSpan;

typedef struct EditorSyntax {
    struct EditorSyntax* next;

    const char* file_type;
    const char* singleline_comment_start;
    const char* multiline_comment_start;
    const char* multiline_comment_end;
    VECTOR(const char*) file_exts;
    VECTOR(const char*) keywords[3];
    uint32_t flags;

    struct JsonValue* value;
} EditorSyntax;

void editorUpdateSyntax(EditorFile* file, EditorRow* row);
void editorSetSyntaxHighlight(EditorFile* file, EditorSyntax* syntax_def);
void editorSelectSyntaxHighlight(EditorFile* file);
void editorInitHLDB(void);
bool editorLoadHLDB(const char* json_file);
void editorFreeHLDB(void);

#endif
