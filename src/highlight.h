#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include "editor.h"
#include "json.h"

typedef struct EditorSyntax {
    struct EditorSyntax* next;

    const char* file_type;
    const char* singleline_comment_start;
    const char* multiline_comment_start;
    const char* multiline_comment_end;
    VECTOR(char*) file_exts;
    VECTOR(char*) keywords[3];
    int flags;

    Arena arena;
    JsonValue* value;
} EditorSyntax;

void editorUpdateSyntax(EditorFile* file, EditorRow* row);
void editorSelectSyntaxHighlight(EditorFile* file);
void editorLoadDefaultHLDB(void);
bool editorLoadHLDB(const char* json_file);
void editorFreeHLDB(void);

#endif
