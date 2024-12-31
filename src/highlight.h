#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include "editor.h"

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
