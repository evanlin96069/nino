#ifndef FILE_TYPE_H
#define FILE_TYPE_H

typedef struct EditorSyntax {
    const char* file_type;
    const char** file_match;
    const char** keywords;
    const char* singleline_comment_start;
    const char* multiline_comment_start;
    const char* multiline_comment_end;
    int flags;
} EditorSyntax;

#define HLDB_ENTRIES 2

extern const EditorSyntax HLDB[HLDB_ENTRIES];

#endif
