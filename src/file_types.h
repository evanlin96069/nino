#ifndef FILE_TYPE_H
#define FILE_TYPE_H

typedef struct EditorSyntax {
    char* file_type;
    char** file_match;
    char** keywords;
    char* singleline_comment_start;
    char* multiline_comment_start;
    char* multiline_comment_end;
    int flags;
} EditorSyntax;

#define HLDB_ENTRIES 2

extern EditorSyntax HLDB[HLDB_ENTRIES];

#endif
