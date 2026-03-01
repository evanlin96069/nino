#ifndef OS_H
#define OS_H

// Terminal
#define READ_WAIT_INFINITE (-1)
#define READ_GRACE_MS 10

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

void osInit(void);

// Terminal
void enableRawMode(void);
void disableRawMode(void);
bool isStdinTty(void);

bool readConsole(uint32_t* unicode_out, int timeout_ms);
int writeConsole(const void* buf, size_t count);
int getWindowSize(int* rows, int* cols);

// File
typedef struct FileInfo FileInfo;
FileInfo getFileInfo(const char* path);
bool areFilesEqual(FileInfo f1, FileInfo f2);
bool isFileModified(FileInfo f1, FileInfo f2);

typedef enum FileType {
    FT_INVALID = -1,
    FT_REG,
    FT_DIR,
    FT_NOT_EXIST,
    FT_ACCESS_DENIED,
    FT_NOT_REG,
} FileType;

FileType getFileType(const char* path);

typedef struct DirIter DirIter;
DirIter dirFindFirst(const char* path);
bool dirNext(DirIter* iter);
void dirClose(DirIter* iter);
const char* dirGetName(const DirIter* iter);
bool pathExists(const char* path);
bool canWriteFile(const char* path);

FILE* openFile(const char* path, const char* mode);
bool shouldSaveInPlace(const char* path);
OsError saveFileInPlace(const char* path, const void* buf, size_t len);
OsError saveFileReplace(const char* path, const void* buf, size_t len);
bool changeDir(const char* path);
char* getFullPath(const char* path);

// Time
int64_t getTime(void);

// Command line
void argsInit(int* argc, char*** argv);
void argsFree(int argc, char** argv);

// Error
void formatOsError(OsError err, char* buf, size_t len);

// Process
void osSuspend(void);
void osRunShell(const char* shell_hint, const char* cmd);

#endif
