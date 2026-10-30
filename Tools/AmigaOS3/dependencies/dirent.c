/* Directory enumeration for the SDK's newlib, which has no dirent API. */
#include "include/dirent.h"
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct OdysseyDirectory {
    BPTR lock;
    struct FileInfoBlock* info;
    struct dirent entry;
};

static int directoryError(void)
{
    switch (IoErr()) {
    case ERROR_OBJECT_NOT_FOUND: case ERROR_DIR_NOT_FOUND: return ENOENT;
    case ERROR_OBJECT_WRONG_TYPE: return ENOTDIR;
    case ERROR_NO_FREE_STORE: return ENOMEM;
    case ERROR_READ_PROTECTED: return EACCES;
    case ERROR_OBJECT_IN_USE: return EBUSY;
    default: return EIO;
    }
}

DIR* opendir(const char* path)
{
    DIR* dir = calloc(1, sizeof(*dir));
    if (!dir)
        return NULL;
    dir->info = AllocDosObject(DOS_FIB, NULL);
    if (!dir->info) {
        free(dir);
        errno = ENOMEM;
        return NULL;
    }
    dir->lock = Lock(path, ACCESS_READ);
    if (!dir->lock || !Examine(dir->lock, dir->info)) {
        int error = directoryError();
        closedir(dir);
        errno = error;
        return NULL;
    }
    if (dir->info->fib_DirEntryType <= 0) {
        closedir(dir);
        errno = ENOTDIR;
        return NULL;
    }
    return dir;
}

struct dirent* readdir(DIR* dir)
{
    if (!dir) {
        errno = EBADF;
        return NULL;
    }
    if (!ExNext(dir->lock, dir->info)) {
        if (IoErr() != ERROR_NO_MORE_ENTRIES)
            errno = directoryError();
        return NULL;
    }
    dir->entry.d_ino = dir->info->fib_DiskKey;
    dir->entry.d_type = dir->info->fib_DirEntryType > 0 ? DT_DIR : DT_REG;
    dir->entry.d_reclen = sizeof(dir->entry);
    memcpy(dir->entry.d_name, dir->info->fib_FileName, sizeof(dir->entry.d_name));
    dir->entry.d_name[sizeof(dir->entry.d_name)-1] = 0;
    return &dir->entry;
}

int closedir(DIR* dir)
{
    if (!dir) {
        errno = EBADF;
        return -1;
    }
    if (dir->lock)
        UnLock(dir->lock);
    FreeDosObject(DOS_FIB, dir->info);
    free(dir);
    return 0;
}

void rewinddir(DIR* dir)
{
    if (!dir || !Examine(dir->lock, dir->info))
        errno = dir ? directoryError() : EBADF;
}
