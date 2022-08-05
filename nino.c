/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>

/*** defines ***/

#define TAB_SIZE 4

#define CTRL_KEY(k) ((k) & 0x1f)

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

enum EditorKey {
    ESC = 27,
    BACKSPACE = 127,
    ARROW_UP = 1000,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
    SHIFT_UP,
    SHIFT_DOWN,
    SHIFT_LEFT,
    SHIFT_RIGHT,
    SHIFT_HOME,
    SHIFT_END,
    CTRL_UP,
    CTRL_DOWN,
    CTRL_LEFT,
    CTRL_RIGHT,
    CTRL_HOME,
    CTRL_END,
    SHIFT_CTRL_UP,
    SHIFT_CTRL_DOWN,
    SHIFT_CTRL_LEFT,
    SHIFT_CTRL_RIGHT
};

enum EditorState {
    EDIT_MODE = 0,
    SAVE_AS_MODE,
    FIND_MODE,
    GOTO_LINE_MODE
};

enum EditorHighlight {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_KEYWORD3,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};

#define DIE(s) die(__LINE__, s)

/*** data ***/

typedef struct EditorRow {
    int idx;
    int size;
    int rsize;
    char* data;
    char* render;
    unsigned char* hl;
    unsigned char* selected;
    int hl_open_comment;
} EditorRow;

typedef struct EditorSyntax {
    char* file_type;
    char** file_match;
    char** keywords;
    char* singleline_comment_start;
    char* multiline_comment_start;
    char* multiline_comment_end;
    int flags;
} EditorSyntax;

typedef struct EditorConfig {
    int cx, cy;
    int rx;
    int sx;
    int px;
    int row_offset;
    int col_offset;
    int rows;
    int cols;
    int num_rows;
    int num_rows_digits;
    EditorRow* row;
    int state;
    int dirty;
    int is_selected;
    int select_x, select_y;
    int bracket_autocomplete;
    char* filename;
    char status_msg[80];
    EditorSyntax* syntax;
    struct termios orig_termios;
} EditorConfig;

EditorConfig E;

/*** file types ***/

char* C_HL_extentions[] = { ".c", ".h", NULL };

char* C_HL_keywords[] = {
    "break", "case", "continue", "default", "do", "else", "for", "goto",
    "if", "return", "switch", "while", "#include", "#define",

    "auto|", "char|", "const|", "double|", "enum|", "extern|", "float|",
    "inline|", "int|", "long|", "register|", "restrict|", "short|",
    "signed|", "sizeof|", "static|", "struct|", "typedef|", "union|",
    "unsigned|", "void|", "volatile|", "bool|", "true|", "false|", "NULL|"

    "int8_t^", "int16_t^", "int32_t^", "int64_t^",
    "uint8_t^", "uint16_t^", "uint32_t^", "uint64_t^",
    "ptrdiff_t^", "size_t", "wchar_t", NULL
};

char* PYTHON_HL_extentions[] = { ".py", NULL };

char* PYTHON_HL_keywords[] = {
    "as", "assert", "break", "continue", "del", "elif", "else", "except",
    "finally", "for", "from", "if", "import", "pass", "raise", "return",
    "try", "while", "with", "yield"

    "and|", "class|", "def|", "False|", "global|", "in|", "is|",
    "lambda|", "None|", "nonlocal|", "not|", "or|", "True|",

    "bool^", "int^", "float^", "complex^", "list^", "tuple^",
    "str^", "bytes^", "bytearray^", "frozenset^", "set^", "dict^",
    "type^", NULL
};

EditorSyntax HLDB[] = {
    {
        "C",
        C_HL_extentions,
        C_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
    {
        "Python",
        PYTHON_HL_extentions,
        PYTHON_HL_keywords,
        "#", "'''", "'''",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    }
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** prototypes ***/

void editorSetStatusMsg(const char* fmt, ...);
void editorRefreshScreen();
char* editorPrompt(char* prompt, int state, void (*callback)(char*, int));

/*** terminal ***/

void die(int line, const char* s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    fprintf(stderr, "Error at line %d: %s\r\n", line, s);
    exit(EXIT_FAILURE);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        DIE("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        DIE("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        DIE("tcsetattr");
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            DIE("read");
    }
    if (c == ESC) {
        char seq[5];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return ESC;
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return ESC;
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return ESC;
                if (seq[2] == '~') {
                    switch (seq[1]) {
                    case '1': return HOME_KEY;
                    case '3': return DEL_KEY;
                    case '4': return END_KEY;
                    case '5': return PAGE_UP;
                    case '6': return PAGE_DOWN;
                    case '7': return HOME_KEY;
                    case '8': return END_KEY;
                    }
                }
                else if (seq[2] == ';') {
                    if (read(STDIN_FILENO, &seq[3], 1) != 1)
                        return ESC;
                    if (read(STDIN_FILENO, &seq[4], 1) != 1)
                        return ESC;
                    if (seq[1] == '1') {
                        if (seq[3] == '2') {
                            // Shift
                            switch (seq[4]) {
                            case 'A': return SHIFT_UP;
                            case 'B': return SHIFT_DOWN;
                            case 'C': return SHIFT_RIGHT;
                            case 'D': return SHIFT_LEFT;
                            case 'H': return SHIFT_HOME;
                            case 'F': return SHIFT_END;
                            }
                        }
                        else if (seq[3] == '5') {
                            // Ctrl
                            switch (seq[4]) {
                            case 'A': return CTRL_UP;
                            case 'B': return CTRL_DOWN;
                            case 'C': return CTRL_RIGHT;
                            case 'D': return CTRL_LEFT;
                            case 'H': return CTRL_HOME;
                            case 'F': return CTRL_END;
                            }
                        }
                        else if (seq[3] == '6') {
                            // Shift + Ctrl
                            switch (seq[4]) {
                            case 'A': return SHIFT_CTRL_UP;
                            case 'B': return SHIFT_CTRL_DOWN;
                            case 'C': return SHIFT_CTRL_RIGHT;
                            case 'D': return SHIFT_CTRL_LEFT;
                            }
                        }
                    }

                }
            }
            else {
                switch (seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                }
            }
        }
        else if (seq[0] == 'O') {
            switch (seq[1]) {
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
            }
        }
    }
    return c;
}

int getCursorPos(int* rows, int* cols) {
    char buf[32];
    size_t i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;
    return 0;
}

int getWindowSize(int* rows, int* cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return getCursorPos(rows, cols);
    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** syntax highlighting ***/


int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*~!^&|=~%<>[]{};?:", c) != NULL;
}

void editorUpdateSyntax(EditorRow* row) {
    row->hl = realloc(row->hl, row->rsize);
    row->selected = realloc(row->selected, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);
    memset(row->hl, 0, row->rsize);

    if (E.syntax == NULL)
        return;

    char** keywords = E.syntax->keywords;

    char* scs = E.syntax->singleline_comment_start;
    char* mcs = E.syntax->multiline_comment_start;
    char* mce = E.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

    int i = 0;
    while (i < row->rsize) {
        char c = row->render[i];
        unsigned char prev_hl = i > 0 ? row->hl[i - 1] : HL_NORMAL;

        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&(row->render[i]), scs, scs_len)) {
                memset(&(row->hl[i]), HL_COMMENT, row->rsize - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&(row->render[i]), mce, mce_len)) {
                    memset(&(row->hl[i]), HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                }
                i++;
                continue;
            }
            else if (!strncmp(&(row->render[i]), mcs, mcs_len)) {
                memset(&(row->hl[i]), HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->rsize) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string)
                    in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            }
            else if (c == '"' || c == '\'') {
                in_string = c;
                row->hl[i] = HL_STRING;
                i++;
                continue;
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
                ((c == '.' || c == 'x' || c == 'X') && prev_hl == HL_NUMBER)) {
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep) {
            int j = 0;
            while (keywords[j]) {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';
                int kw3 = keywords[j][klen - 1] == '^';
                if (kw2 || kw3)
                    klen--;
                int keyword_type = HL_KEYWORD1;
                if (kw2) {
                    keyword_type = HL_KEYWORD2;
                }
                else if (kw3) {
                    keyword_type = HL_KEYWORD3;
                }
                if (!strncmp(&(row->render[i]), keywords[j], klen) &&
                    is_separator(row->render[i + klen])) {
                    memset(&(row->hl[i]), keyword_type, klen);
                    i += klen;
                    break;
                }
                j++;
            }
            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }
        prev_sep = is_separator(c);
        i++;
    }
    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < E.num_rows)
        editorUpdateSyntax(&(E.row[row->idx + 1]));
}

int editorSyntaxToColor(int hl) {
    switch (hl) {
    case HL_COMMENT:
    case HL_MLCOMMENT: return 32;
    case HL_KEYWORD1: return 35;
    case HL_KEYWORD2: return 36;
    case HL_KEYWORD3: return 92;
    case HL_STRING: return 33;
    case HL_NUMBER: return 93;
    case HL_MATCH: return 31;
    default: return 37;
    }
}

void editorSelectSyntaxHighlight() {
    E.syntax = NULL;
    if (E.filename == NULL)
        return;

    char* ext = strrchr(E.filename, '.');

    for (unsigned int i = 0; i < HLDB_ENTRIES; i++) {
        EditorSyntax* s = &HLDB[i];
        unsigned int j = 0;
        while (s->file_match[j]) {
            int is_ext = (s->file_match[j][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->file_match[j])) ||
                (!is_ext && strstr(E.filename, s->file_match[j]))) {
                E.syntax = s;

                for (int file_row = 0; file_row < E.num_rows; file_row++) {
                    editorUpdateSyntax(&(E.row[file_row]));
                }

                return;
            }
            j++;
        }
    }
}

/*** row operations ***/

int editorRowCxToRx(EditorRow* row, int cx) {
    int rx = 0;
    for (int i = 0; i < cx; i++) {
        if (row->data[i] == '\t') {
            rx += (TAB_SIZE - 1) - (rx % TAB_SIZE);
        }
        rx++;
    }
    return rx;
}

int editorRowRxToCx(EditorRow* row, int rx) {
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < row->size; cx++) {
        if (row->data[cx] == '\t')
            cur_rx += (TAB_SIZE - 1) - (cur_rx % TAB_SIZE);
        cur_rx++;
        if (cur_rx > rx)
            return cx;
    }
    return cx;
}

int editorRowSxToCx(EditorRow* row, int sx) {
    if (sx <= 0)
        return 0;
    int cx = 0;
    int rx = 0;
    int rx2 = 0;
    while (cx < row->size && rx < sx) {
        rx2 = rx;
        if (row->data[cx] == '\t') {
            rx += (TAB_SIZE - 1) - (rx % TAB_SIZE);
        }
        rx++;
        cx++;
    }
    if (rx - sx >= sx - rx2) {
        cx--;
    }
    return cx;
}

void editorUpdateRow(EditorRow* row) {
    int tabs = 0;
    for (int i = 0; i < row->size; i++) {
        if (row->data[i] == '\t')
            tabs++;
    }

    free(row->render);
    row->render = malloc(row->size + tabs * (TAB_SIZE - 1) + 1);

    int idx = 0;
    for (int i = 0; i < row->size; i++) {
        if (row->data[i] == '\t') {
            row->render[idx++] = ' ';
            while (idx % TAB_SIZE != 0)
                row->render[idx++] = ' ';
        }
        else {
            row->render[idx++] = row->data[i];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;

    editorUpdateSyntax(row);
}

void editorInsertRow(int at, char* s, size_t len) {
    if (at < 0 || at > E.num_rows)
        return;

    E.row = realloc(E.row, sizeof(EditorRow) * (E.num_rows + 1));
    memmove(&(E.row[at + 1]), &(E.row[at]), sizeof(EditorRow) * (E.num_rows - at));
    for (int i = at + 1; i <= E.num_rows; i++) {
        E.row[i].idx++;
    }

    E.row[at].idx = at;

    E.row[at].size = len;
    E.row[at].data = malloc(len + 1);
    memcpy(E.row[at].data, s, len);
    E.row[at].data[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].selected = NULL;
    E.row[at].hl_open_comment = 0;
    editorUpdateRow(&(E.row[at]));

    E.num_rows++;
    E.dirty++;

    E.num_rows_digits = 0;
    int num_rows = E.num_rows;
    while (num_rows) {
        num_rows /= 10;
        E.num_rows_digits++;
    }
}

void editorFreeRow(EditorRow* row) {
    free(row->render);
    free(row->data);
    free(row->hl);
    free(row->selected);
}

void editorDelRow(int at) {
    if (at < 0 || at >= E.num_rows)
        return;
    editorFreeRow(&(E.row[at]));
    memmove(&(E.row[at]), &(E.row[at + 1]), sizeof(EditorRow) * (E.num_rows - at - 1));
    for (int i = at; i < E.num_rows - 1; i++) {
        E.row[i].idx--;
    }
    E.num_rows--;
    E.dirty++;

    E.num_rows_digits = 0;
    int num_rows = E.num_rows;
    while (num_rows) {
        num_rows /= 10;
        E.num_rows_digits++;
    }
}

void editorRowInsertChar(EditorRow* row, int at, int c) {
    if (at < 0 || at > row->size)
        at = row->size;
    row->data = realloc(row->data, row->size + 2);
    memmove(&(row->data[at + 1]), &(row->data[at]), row->size - at + 1);
    row->size++;
    row->data[at] = c;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDelChar(EditorRow* row, int at) {
    if (at < 0 || at >= row->size)
        return;
    memmove(&(row->data[at]), &(row->data[at + 1]), row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowAppendString(EditorRow* row, char* s, size_t len) {
    row->data = realloc(row->data, row->size + len + 1);
    memcpy(&(row->data[row->size]), s, len);
    row->size += len;
    row->data[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

/*** editor operations ***/

void editorInsertChar(int c) {
    if (E.cy == E.num_rows) {
        editorInsertRow(E.num_rows, "", 0);
    }
    editorRowInsertChar(&(E.row[E.cy]), E.cx, c);
    E.cx++;
}

void editorInsertNewline() {
    int i = 0;

    if (E.cx == 0) {
        editorInsertRow(E.cy, "", 0);
    }
    else {
        editorInsertRow(E.cy + 1, "", 0);
        EditorRow* curr_row = &(E.row[E.cy]);
        EditorRow* new_row = &(E.row[E.cy + 1]);

        while (i < E.cx && (curr_row->data[i] == ' ' || curr_row->data[i] == '\t'))
            i++;
        if (i != 0)
            editorRowAppendString(new_row, curr_row->data, i);
        if (curr_row->data[E.cx - 1] == ':' ||
            (curr_row->data[E.cx - 1] == '{' && curr_row->data[E.cx] != '}')) {
            editorRowAppendString(new_row, "\t", 1);
            i++;
        }
        editorRowAppendString(new_row, &(curr_row->data[E.cx]), curr_row->size - E.cx);
        curr_row->size = E.cx;
        curr_row->data[curr_row->size] = '\0';
        editorUpdateRow(curr_row);
    }
    E.cy++;
    E.cx = i;
    E.sx = editorRowCxToRx(&(E.row[E.cy]), i);
}

void editorDelChar() {
    if (E.cy == E.num_rows)
        return;
    if (E.cx == 0 && E.cy == 0)
        return;
    EditorRow* row = &(E.row[E.cy]);
    if (E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    }
    else {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&(E.row[E.cy - 1]), row->data, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
    E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
}

/*** select text ***/

void getSelectStartEnd(int* start_x, int* start_y, int* end_x, int* end_y) {
    if (E.select_y > E.cy) {
        *start_x = E.cx;
        *start_y = E.cy;
        *end_x = E.select_x;
        *end_y = E.select_y;
    }
    else if (E.select_y < E.cy) {
        *start_x = E.select_x;
        *start_y = E.select_y;
        *end_x = E.cx;
        *end_y = E.cy;
    }
    else {
        // same row
        *start_y = *end_y = E.cy;
        *start_x = E.select_x > E.cx ? E.cx : E.select_x;
        *end_x = E.select_x > E.cx ? E.select_x : E.cx;
    }
}

void editorSelectText() {
    if (!E.is_selected)
        return;
    for (int i = 0; i < E.num_rows; i++) {
        memset(E.row[i].selected, 0, E.row[i].rsize);
    }
    int start_x, start_y, end_x, end_y;
    getSelectStartEnd(&start_x, &start_y, &end_x, &end_y);
    start_x = editorRowCxToRx(&(E.row[start_y]), start_x);
    end_x = editorRowCxToRx(&(E.row[end_y]), end_x);

    if (start_y == end_y) {
        memset(&(E.row[E.cy].selected[start_x]), 1, end_x - start_x);
        return;
    }

    for (int i = start_y; i <= end_y; i++) {
        if (i == start_y) {
            memset(&(E.row[i].selected[start_x]), 1, E.row[i].rsize - start_x);
        }
        else if (i == end_y) {
            memset(E.row[i].selected, 1, end_x);
        }
        else {
            memset(E.row[i].selected, 1, E.row[i].rsize);
        }
    }

}

void editorDeleteSelectText() {
    int start_x, start_y, end_x, end_y;
    getSelectStartEnd(&start_x, &start_y, &end_x, &end_y);
    E.cx = end_x;
    E.cy = end_y;
    if (end_y - start_y > 1) {
        for (int i = start_y + 1; i < end_y; i++) {
            editorFreeRow(&(E.row[i]));
        }
        int removed_rows = end_y - start_y - 1;
        memmove(&(E.row[start_y + 1]), &(E.row[end_y]), sizeof(EditorRow) * (E.num_rows - end_y));
        for (int i = start_y + 1; i < E.num_rows - removed_rows; i++) {
            E.row[i].idx -= removed_rows;
        }
        E.num_rows -= removed_rows;
        E.cy -= removed_rows;
        E.dirty++;

        E.num_rows_digits = 0;
        int num_rows = E.num_rows;
        while (num_rows) {
            num_rows /= 10;
            E.num_rows_digits++;
        }
    }
    while (E.cy != start_y || E.cx != start_x) {
        editorDelChar();
    }

}

/*** find ***/

void editorFindCallback(char* query, int key) {
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static unsigned char* saved_hl = NULL;

    if (saved_hl) {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    int len = strlen(query);
    if (len == 0)
        return;

    if (key == ESC || (last_match != -1 && key == '\r')) {
        last_match = -1;
        direction = 1;
        return;
    }
    if (key == ARROW_RIGHT || key == ARROW_DOWN || key == '\r') {
        direction = 1;
    }
    else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    }
    else {
        last_match = -1;
    }

    if (last_match == -1)
        direction = 1;
    int current = last_match;
    for (int i = 0; i < E.num_rows; i++) {
        current += direction;
        if (current == -1)
            current = E.num_rows - 1;
        else if (current == E.num_rows)
            current = 0;
        EditorRow* row = &(E.row[current]);
        char* match = strstr(row->render, query);
        if (match) {
            match += len;
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row, match - row->render);
            E.sx = match - row->render;
            E.row_offset = E.num_rows;
            if (key == '\r') {
                last_match = -1;
                direction = 1;
                return;
            }
            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&(row->hl[match - len - row->render]), HL_MATCH, len);
            break;
        }
    }
}

void editorFind() {
    char* query = editorPrompt("Search: %s", FIND_MODE, editorFindCallback);
    if (query) {
        free(query);
    }
}

/*** go to line ***/

void editorGotoLine() {
    char* query = editorPrompt("Goto line: %s", GOTO_LINE_MODE, NULL);
    if (query == NULL)
        return;
    int line = atoi(query);
    if (line < 0) {
        line = E.num_rows + 1 + line;
    }
    if (line > 0 && line <= E.num_rows) {
        E.cx = 0;
        E.sx = 0;
        E.cy = line - 1;
    }
    if (query) {
        free(query);
    }
}

/*** file IO ***/

char* editroRowsToString(int* len) {
    int total_len = 0;
    for (int i = 0; i < E.num_rows; i++) {
        total_len += E.row[i].size + 1;
    }
    // last line no newline
    *len = total_len - 1;

    char* buf = malloc(total_len);
    char* p = buf;
    for (int i = 0; i < E.num_rows; i++) {
        memcpy(p, E.row[i].data, E.row[i].size);
        p += E.row[i].size;
        if (i != E.num_rows - 1)
            *p = '\n';
        p++;
    }

    return buf;
}

void editorOpen(char* filename) {
    free(E.filename);
    size_t fnlen = strlen(filename) + 1;
    E.filename = malloc(fnlen);
    memcpy(E.filename, filename, fnlen);

    editorSelectSyntaxHighlight();

    FILE* f = fopen(filename, "r");
    if (f) {
        char* line = NULL;
        size_t cap = 0;
        ssize_t len;
        while ((len = getline(&line, &cap, f)) != -1) {
            while (len > 0 && (line[len - 1] == '\n' ||
                line[len - 1] == '\r'))
                len--;
            editorInsertRow(E.num_rows, line, len);
        }
        free(line);
        fclose(f);
    }
    else {
        editorInsertRow(E.cy, "", 0);
    }
    E.dirty = 0;
}

void editorSave() {
    if (E.filename == NULL) {
        E.filename = editorPrompt("Save as: %s", SAVE_AS_MODE, NULL);
        if (E.filename == NULL) {
            editorSetStatusMsg("Save aborted.");
            return;
        }
        editorSelectSyntaxHighlight();
    }
    int len;
    char* buf = editroRowsToString(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMsg("%d bytes written to disk.", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMsg("Can't save! I/O error: %s", strerror(errno));
}

/*** append buffer ***/

typedef struct {
    char* buf;
    int len;
} abuf;

#define ABUF_INIT {NULL, 0}

void abufAppend(abuf* ab, const char* s, int len) {
    char* new = realloc(ab->buf, ab->len + len);

    if (new == NULL)
        return;
    memcpy(&new[ab->len], s, len);
    ab->buf = new;
    ab->len += len;
}

void abufFree(abuf* ab) {
    free(ab->buf);
}

/*** output ***/

void editorScroll() {
    E.rx = 0;
    if (E.cy < E.num_rows) {
        E.rx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
    }

    if (E.cy < E.row_offset) {
        E.row_offset = E.cy;
    }
    if (E.cy >= E.row_offset + E.rows) {
        E.row_offset = E.cy - E.rows + 1;
    }
    if (E.rx < E.col_offset) {
        E.col_offset = E.rx;
    }
    if (E.rx >= E.col_offset + E.cols) {
        E.col_offset = E.rx - E.cols + 1;
    }
}

int editorBuildNumber() {
    struct tm tm = { 0 };
    time_t epoch;
    if (strptime(__DATE__, "%b %d %Y", &tm) == NULL)
        return 0;
    epoch = mktime(&tm);
    // Sep 13 2020
    return (epoch / 86400) - 18518;
}

void editorDrawTopStatusBar(abuf* ab) {
    int cols = E.cols + E.num_rows_digits + 1;
    abufAppend(ab, "\x1b[48;5;234m", 11);
    char status[80];
    int len = snprintf(status, sizeof(status),
        "  Nino Editor (build %d)",
        editorBuildNumber());
    char rstatus[80];
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s%.20s",
        E.dirty ? "*" : "",
        E.filename ? E.filename : "Untitled");
    if (len > cols)
        len = cols;
    abufAppend(ab, status, len);

    for (int i = len; i < cols; i++) {
        if (i == (cols - rlen) / 2) {
            abufAppend(ab, rstatus, rlen);
            i += rlen;
        }
        abufAppend(ab, " ", 1);
    }
    abufAppend(ab, "\x1b[m", 3);
    abufAppend(ab, "\r\n", 2);
}

void editorDrawStatusBar(abuf* ab) {
    int cols = E.cols + E.num_rows_digits + 1;
    abufAppend(ab, "\x1b[48;5;53m", 10);
    char rstatus[80];
    const char* help_info[] = {
        " ^Q: Quit  ^S: Save  ^F: Find ^G: Goto",
        " ^Q: Cancel",
        " ^Q: Cancel  ◀: back  ▶: Next",
        " ^Q: Cancel"
    };
    int len = strlen(help_info[E.state]);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | Ln: %d, Col: %d  ",
        E.syntax ? E.syntax->file_type : "Plain Text",
        E.cy + 1, E.rx + 1);
    if (len > cols)
        len = cols;

    abufAppend(ab, help_info[E.state], len);

    while (len < cols) {
        if (cols - len == rlen) {
            abufAppend(ab, rstatus, rlen);
            break;
        }
        else {
            abufAppend(ab, " ", 1);
            len++;
        }
    }
    abufAppend(ab, "\x1b[m", 3);
}

void editorDrawStatusMsgBar(abuf* ab) {
    int cols = E.cols + E.num_rows_digits + 1;
    abufAppend(ab, "\x1b[K", 3);
    int len = strlen(E.status_msg);
    if (len > cols)
        len = cols;
    if (len)
        abufAppend(ab, E.status_msg, len);

    abufAppend(ab, "\r\n", 2);
}

void editorDrawRows(abuf* ab) {
    editorSelectText();
    for (int i = 0; i < E.rows; i++) {
        int current_row = i + E.row_offset;
        if (current_row >= E.num_rows) {
            abufAppend(ab, "~", 1);
        }
        else {
            char line_number[16];
            if (current_row == E.cy) {
                abufAppend(ab, "\x1b[30;100m", 9);
            }
            else {
                abufAppend(ab, "\x1b[90m", 5);
            }
            int nlen = snprintf(line_number, sizeof(line_number), "%*d ", E.num_rows_digits, current_row + 1);
            abufAppend(ab, line_number, nlen);
            abufAppend(ab, "\x1b[m", 3);
            int len = E.row[current_row].rsize - E.col_offset;
            if (len < 0)
                len = 0;
            if (len > E.cols)
                len = E.cols;
            char* c = &(E.row[current_row].render[E.col_offset]);
            unsigned char* hl = &(E.row[current_row].hl[E.col_offset]);
            unsigned char* selected = &(E.row[current_row].selected[E.col_offset]);
            int current_color = -1;
            for (int j = 0; j < len; j++) {
                if (iscntrl(c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abufAppend(ab, "\x1b[7m", 4);
                    abufAppend(ab, &sym, 1);
                    abufAppend(ab, "\x1b[m", 3);
                    if (current_color != -1) {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        abufAppend(ab, buf, clen);
                    }
                }
                else if (E.is_selected && selected[j]) {
                    current_color = -2;
                    abufAppend(ab, "\x1b[30;47m", 8);
                    abufAppend(ab, &c[j], 1);
                    abufAppend(ab, "\x1b[m", 3);

                }
                else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        abufAppend(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    abufAppend(ab, &c[j], 1);
                }
                else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        abufAppend(ab, buf, clen);
                    }
                    abufAppend(ab, &c[j], 1);
                }
            }
            abufAppend(ab, "\x1b[39m", 5);
        }

        abufAppend(ab, "\x1b[K", 3);
        abufAppend(ab, "\r\n", 2);
    }
}

void editorRefreshScreen() {
    editorScroll();

    abuf ab = ABUF_INIT;

    abufAppend(&ab, "\x1b[?25l", 6);
    abufAppend(&ab, "\x1b[H", 3);

    editorDrawTopStatusBar(&ab);
    editorDrawRows(&ab);
    editorDrawStatusMsgBar(&ab);
    editorDrawStatusBar(&ab);

    char buf[32];
    if (E.state == EDIT_MODE) {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
            (E.cy - E.row_offset) + 2,
            (E.rx - E.col_offset) + 1 + E.num_rows_digits + 1);
    }
    else {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
            E.rows + 2,
            E.px + 1);
    }

    abufAppend(&ab, buf, strlen(buf));

    abufAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buf, ab.len);
    abufFree(&ab);
}

void editorSetStatusMsg(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.status_msg, sizeof(E.status_msg), fmt, ap);
    va_end(ap);
}

/*** input ***/

char* editorPrompt(char* prompt, int state, void (*callback)(char*, int)) {
    int prev_state = E.state;
    E.state = state;

    size_t bufsize = 128;
    char* buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    int start = 0;
    while (prompt[start] != '\0' && prompt[start] != '%') {
        start++;
    }
    E.px = start;
    while (1) {
        editorSetStatusMsg(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        switch (c) {
        case DEL_KEY:
        case CTRL_KEY('h'):
        case BACKSPACE:
            if (buflen != 0) {
                buf[--buflen] = '\0';
                E.px--;
                if (callback)
                    callback(buf, c);
            }
            break;

        case CTRL_KEY('q'):
        case ESC:
            editorSetStatusMsg("");
            free(buf);
            E.state = prev_state;
            if (callback)
                callback(buf, c);
            return NULL;

        case '\r':
            if (buflen != 0) {
                editorSetStatusMsg("");
                E.state = prev_state;
                if (callback)
                    callback(buf, c);
                return buf;
            }
            break;

        default:
            if (!iscntrl(c) && c < 128) {
                if (buflen == bufsize - 1) {
                    bufsize *= 2;
                    buf = realloc(buf, bufsize);
                }
                buf[buflen++] = c;
                buf[buflen] = '\0';
                E.px++;
            }
            if (callback)
                callback(buf, c);
            break;
        }
        if (getWindowSize(&E.rows, &E.cols) == -1)
            DIE("getWindowSize");
        E.rows -= 3;
        E.cols -= E.num_rows_digits + 1;
    }
}

void editorMoveCursor(int key) {
    EditorRow* row = (E.cy >= E.num_rows) ? NULL : &(E.row[E.cy]);
    switch (key) {
    case ARROW_LEFT:
        if (E.cx != 0) {
            E.cx--;
            E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
        }
        else if (E.cy > 0) {
            E.cy--;
            E.cx = E.row[E.cy].size;
            E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
        }
        break;
    case ARROW_RIGHT:
        if (row && E.cx < row->size) {
            E.cx++;
            E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
        }
        else if (row && (E.cy + 1 < E.num_rows) && E.cx == row->size) {
            E.cy++;
            E.cx = 0;
            E.sx = 0;
        }

        break;
    case ARROW_UP:
        if (E.cy != 0) {
            E.cy--;
            E.cx = editorRowSxToCx(&(E.row[E.cy]), E.sx);
        }
        break;
    case ARROW_DOWN:
        if (E.cy + 1 < E.num_rows) {
            E.cy++;
            E.cx = editorRowSxToCx(&(E.row[E.cy]), E.sx);
        }
        break;
    }
    row = (E.cy >= E.num_rows) ? NULL : &(E.row[E.cy]);
    int row_len = row ? row->size : 0;
    if (E.cx > row_len) {
        E.cx = row_len;
    }
}

char isOpenBracket(int key) {
    switch (key) {
    case '(': return ')';
    case '[': return ']';
    case '{': return '}';
    default: return 0;
    }
}

char isCloseBracket(int key) {
    switch (key) {
    case ')': return '(';
    case ']': return '[';
    case '}': return '{';
    default: return 0;
    }
}

void editorProcessKeypress() {
    static int quit_protect = 1;
    int c = editorReadKey();
    editorSetStatusMsg("");
    switch (c) {
    case '\r':
        if (E.is_selected) {
            editorDeleteSelectText();
            E.is_selected = 0;
        }
        E.bracket_autocomplete = 0;

        editorInsertNewline();
        break;

    case CTRL_KEY('q'):
        if (E.dirty && quit_protect) {
            editorSetStatusMsg("File has unsaved changes. Press ^Q again to quit anyway.");
            quit_protect = 0;
            return;
        }
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(EXIT_SUCCESS);
        break;

    case CTRL_KEY('s'):
        if (E.dirty)
            editorSave();
        break;

    case HOME_KEY:
    case CTRL_LEFT:
    case SHIFT_HOME:
    case SHIFT_CTRL_LEFT:
        if (E.cx == 0)
            break;
        E.cx = 0;
        E.sx = 0;
        E.is_selected = (c == SHIFT_HOME || c == SHIFT_CTRL_LEFT);
        E.bracket_autocomplete = 0;
        break;
    case END_KEY:
    case CTRL_RIGHT:
    case SHIFT_END:
    case SHIFT_CTRL_RIGHT:
        if (E.cy < E.num_rows && E.cx != E.row[E.cy].size) {
            E.cx = E.row[E.cy].size;
            E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
            E.is_selected = (c == SHIFT_END || c == SHIFT_CTRL_RIGHT);
            E.bracket_autocomplete = 0;
        }
        break;

    case CTRL_KEY('f'):
        E.is_selected = 0;
        E.bracket_autocomplete = 0;
        editorFind();
        break;

    case CTRL_KEY('g'):
        E.is_selected = 0;
        E.bracket_autocomplete = 0;
        editorGotoLine();
        break;

    case CTRL_KEY('a'):
        if (E.num_rows == 1 && E.row[0].size == 0)
            break;
        E.is_selected = 1;
        E.bracket_autocomplete = 0;
        E.cy = E.num_rows - 1;
        E.cx = E.row[E.num_rows - 1].size;
        E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
        E.select_y = 0;
        E.select_x = 0;
        break;

    case DEL_KEY:
    case CTRL_KEY('h'):
    case BACKSPACE:
        if (E.is_selected) {
            editorDeleteSelectText();
            E.is_selected = 0;
            break;
        }
        if (c == DEL_KEY)
            editorMoveCursor(ARROW_RIGHT);
        else if (E.bracket_autocomplete &&
            (isCloseBracket(E.row[E.cy].data[E.cx]) == E.row[E.cy].data[E.cx - 1] ||
                (E.row[E.cy].data[E.cx] == '\'' && E.row[E.cy].data[E.cx - 1] == '\'') ||
                (E.row[E.cy].data[E.cx] == '"' && E.row[E.cy].data[E.cx - 1] == '"'))
            ) {
            E.bracket_autocomplete--;
            editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
        }
        editorDelChar();
        break;

    case PAGE_UP:
    case PAGE_DOWN:
        E.is_selected = 0;
        E.bracket_autocomplete = 0;
        {
            if (c == PAGE_UP) {
                E.cy = E.row_offset;
            }
            else if (c == PAGE_DOWN) {
                E.cy = E.row_offset + E.rows - 1;
                if (E.cy >= E.num_rows)
                    E.cy = E.num_rows - 1;
            }
            int times = E.rows;
            while (times--) {
                if (c == PAGE_UP) {
                    if (E.cy == 0) {
                        E.cx = 0;
                        E.sx = 0;
                        break;
                    }
                    editorMoveCursor(ARROW_UP);
                }
                else {
                    if (E.cy == E.num_rows - 1) {
                        E.cx = E.row[E.cy].size;
                        break;
                    }
                    editorMoveCursor(ARROW_DOWN);
                }
            }
        }
        break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        if (E.is_selected) {
            int start_x, start_y, end_x, end_y;
            getSelectStartEnd(&start_x, &start_y, &end_x, &end_y);

            if (c == ARROW_UP || c == ARROW_LEFT) {
                E.cx = start_x;
                E.cy = start_y;
            }
            else {
                E.cx = end_x;
                E.cy = end_y;
            }
            E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
            if (c == ARROW_UP || c == ARROW_DOWN) {
                editorMoveCursor(c);
            }
            E.is_selected = 0;
        }
        else {
            if (E.bracket_autocomplete) {
                if (ARROW_RIGHT)
                    E.bracket_autocomplete--;
                else
                    E.bracket_autocomplete = 0;
            }
            editorMoveCursor(c);
        }
        break;

    case SHIFT_UP:
    case SHIFT_DOWN:
    case SHIFT_LEFT:
    case SHIFT_RIGHT:
        E.is_selected = 1;
        E.bracket_autocomplete = 0;
        editorMoveCursor(c - 9);
        break;

    case CTRL_UP:
    case CTRL_HOME:
        E.is_selected = 0;
        E.bracket_autocomplete = 0;
        E.cy = 0;
        E.cx = 0;
        E.sx = 0;
        break;
    case CTRL_DOWN:
    case CTRL_END:
        E.is_selected = 0;
        E.bracket_autocomplete = 0;
        E.cy = E.num_rows - 1;
        E.cx = E.row[E.num_rows - 1].size;
        E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
        break;

    case SHIFT_CTRL_UP:
    case SHIFT_CTRL_DOWN:

    case CTRL_KEY('l'):
    case ESC:
        break;

    default:
        if (isprint(c) || c == '\t') {
            if (E.is_selected) {
                editorDeleteSelectText();
                E.is_selected = 0;
            }
            int close_bracket = isOpenBracket(c);
            int open_bracket = isCloseBracket(c);
            if (close_bracket) {
                editorInsertChar(c);
                editorInsertChar(close_bracket);
                E.cx--;
                E.bracket_autocomplete++;
            }
            else if (open_bracket) {
                if (E.bracket_autocomplete && E.row[E.cy].data[E.cx] == c) {
                    E.bracket_autocomplete--;
                    E.cx++;
                }
                else {
                    editorInsertChar(c);
                }
            }
            else if (c == '\'' || c == '"') {
                if (E.row[E.cy].data[E.cx] != c) {
                    editorInsertChar(c);
                    editorInsertChar(c);
                    E.cx--;
                    E.bracket_autocomplete++;
                }
                else if (E.bracket_autocomplete && E.row[E.cy].data[E.cx] == c) {
                    E.bracket_autocomplete--;
                    E.cx++;
                }
                else {
                    editorInsertChar(c);
                }
            }
            else {
                editorInsertChar(c);
            }
            E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
        }
        E.is_selected = 0;
        break;
    }
    if (!E.is_selected) {
        E.select_x = E.cx;
        E.select_y = E.cy;
    }
    quit_protect = 1;
}

/*** init ***/

void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.sx = 0;
    E.row_offset = 0;
    E.col_offset = 0;
    E.num_rows = 0;
    E.num_rows_digits = 0;
    E.row = NULL;
    E.is_selected = 0;
    E.select_x = 0;
    E.select_y = 0;
    E.state = EDIT_MODE;
    E.dirty = 0;
    E.bracket_autocomplete = 0;
    E.filename = NULL;
    E.status_msg[0] = '\0';
    E.syntax = 0;

    if (getWindowSize(&E.rows, &E.cols) == -1)
        DIE("getWindowSize");

    E.rows -= 3;
}

int main(int argc, char* argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }
    else {
        editorInsertRow(E.cy, "", 0);
        E.dirty = 0;
    }
    E.cols -= E.num_rows_digits + 1;
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
        if (getWindowSize(&E.rows, &E.cols) == -1)
            DIE("getWindowSize");
        E.rows -= 3;
        E.cols -= E.num_rows_digits + 1;
    }
    return 0;
}
