#ifndef OS_H
#define OS_H

void osInit(void);

// Terminal
void enableRawMode(void);
void disableRawMode(void);
bool readConsole(uint32_t* unicode);
int getWindowSize(int* rows, int* cols);

// File
typedef struct FileInfo FileInfo;
FileInfo getFileInfo(const char* path);
bool areFilesEqual(FileInfo f1, FileInfo f2);

typedef enum FileType {
    FT_INVALID = -1,
    FT_REG,
    FT_DIR,
    FT_DEV,
} FileType;
FileType getFileType(const char* path);

typedef struct DirIter DirIter;
DirIter dirFindFirst(const char* path);
bool dirNext(DirIter* iter);
void dirClose(DirIter* iter);
const char* dirGetName(const DirIter* iter);

FILE* openFile(const char* path, const char* mode);
bool changeDir(const char* path);
char* getFullPath(const char* path);

// Time
int64_t getTime(void);

// Command line
void argsInit(int* argc, char*** argv);
void argsFree(int argc, char** argv);

// New Line
#define NL_UNIX 0
#define NL_DOS 1

#ifdef _WIN32
#define NL_DEFAULT NL_DOS
#include "os_win32.h"
#else
#define NL_DEFAULT NL_UNIX
#include "os_unix.h"
#endif

#endif
