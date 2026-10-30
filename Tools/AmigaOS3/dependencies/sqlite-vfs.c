/* SQLite rollback-journal VFS for classic AmigaDOS. No POSIX fd emulation. */
#define _DEFAULT_SOURCE 1
#include "sqlite3.h"
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/record.h>
#include <proto/dos.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 1024
#define PENDING_BYTE 0x40000000UL
#define RESERVED_BYTE (PENDING_BYTE + 1)
#define SHARED_FIRST (PENDING_BYTE + 2)
#define SHARED_SIZE 510

extern int odysseyAmigaOS3RandomBytes(unsigned char*, size_t);

typedef struct {
    sqlite3_file base;
    BPTR handle;
    int readOnly, deleteOnClose;
    int lockLevel, shared, reserved, pending, exclusive;
    char path[MAX_PATH];
} AmigaFile;

static int ioError(int fallback)
{
    switch (IoErr()) {
    case ERROR_WRITE_PROTECTED: case ERROR_DISK_WRITE_PROTECTED:
        return SQLITE_READONLY;
    case ERROR_DISK_FULL: return SQLITE_FULL;
    case ERROR_LOCK_COLLISION: case ERROR_LOCK_TIMEOUT: case ERROR_OBJECT_IN_USE:
        return SQLITE_BUSY;
    default: return fallback;
    }
}

static int lockRange(AmigaFile* file, ULONG offset, ULONG size, ULONG mode)
{
    int result = LockRecord(file->handle, offset, size, mode, 0) ? SQLITE_OK : ioError(SQLITE_IOERR_LOCK);
#ifdef SQLITE_AMIGAOS3_TRACE
    printf("lock %s h=%lx offset=%lx length=%lu mode=%lu result=%d dos=%ld\n", file->path, (long)file->handle, offset, size, mode, result, (long)IoErr());
#endif
    return result;
}

static int unlockRange(AmigaFile* file, ULONG offset, ULONG size)
{
    int result = UnLockRecord(file->handle, offset, size) ? SQLITE_OK : SQLITE_IOERR_UNLOCK;
#ifdef SQLITE_AMIGAOS3_TRACE
    printf("unlock %s h=%lx offset=%lx length=%lu result=%d dos=%ld\n", file->path, (long)file->handle, offset, size, result, (long)IoErr());
#endif
    return result;
}

static int amigaUnlock(sqlite3_file* base, int level)
{
    AmigaFile* file = (AmigaFile*)base;
    int result = SQLITE_OK;
    if (file->exclusive) {
        result = unlockRange(file, SHARED_FIRST, SHARED_SIZE);
        if (result) return result;
        file->exclusive = 0;
        if (level == SQLITE_LOCK_SHARED) {
            result = lockRange(file, SHARED_FIRST, SHARED_SIZE, REC_SHARED_IMMED);
            if (result) return result;
            file->shared = 1;
        }
    }
    if (file->shared && level == SQLITE_LOCK_NONE) {
        result = unlockRange(file, SHARED_FIRST, SHARED_SIZE);
        if (result) return result;
        file->shared = 0;
    }
    if (file->reserved) {
        result = unlockRange(file, RESERVED_BYTE, 1);
        if (result) return result;
        file->reserved = 0;
    }
    if (file->pending) {
        result = unlockRange(file, PENDING_BYTE, 1);
        if (result) return result;
        file->pending = 0;
    }
    file->lockLevel = level;
    return SQLITE_OK;
}

static int amigaClose(sqlite3_file* base)
{
    AmigaFile* file = (AmigaFile*)base;
    int result = amigaUnlock(base, SQLITE_LOCK_NONE);
    if (!Close(file->handle) && !result) result = SQLITE_IOERR_CLOSE;
    if (file->deleteOnClose && !DeleteFile(file->path) && !result)
        result = SQLITE_IOERR_DELETE;
    file->handle = 0;
    base->pMethods = NULL;
    return result;
}

static int seekTo(AmigaFile* file, sqlite3_int64 offset, int amount)
{
    if (offset < 0 || offset > INT32_MAX || amount < 0 || offset + amount > INT32_MAX)
        return SQLITE_FULL;
    SetIoErr(0);
    if (Seek(file->handle, (LONG)offset, OFFSET_BEGINNING) == -1 && IoErr())
        return SQLITE_IOERR_SEEK;
    return SQLITE_OK;
}

static int amigaFileSize(sqlite3_file*, sqlite3_int64*);

static int amigaRead(sqlite3_file* base, void* data, int amount, sqlite3_int64 offset)
{
    AmigaFile* file = (AmigaFile*)base;
    sqlite3_int64 size;
    int sizeResult = amigaFileSize(base, &size);
    if (sizeResult) return sizeResult;
    /* DOS Seek rejects positions beyond EOF; SQLite requires a zero-filled
     * short read, including reads wholly past the end of a new database. */
    if (offset >= size) {
        memset(data, 0, amount);
        return SQLITE_IOERR_SHORT_READ;
    }
    int result = seekTo(file, offset, amount);
    if (result) return result;
    LONG count = Read(file->handle, data, amount);
    if (count < 0) return SQLITE_IOERR_READ;
    if (count != amount) {
        memset((char*)data + count, 0, amount - count);
        return SQLITE_IOERR_SHORT_READ;
    }
    return SQLITE_OK;
}

static int amigaWrite(sqlite3_file* base, const void* data, int amount, sqlite3_int64 offset)
{
    AmigaFile* file = (AmigaFile*)base;
    if (file->readOnly) return SQLITE_READONLY;
    sqlite3_int64 size;
    int sizeResult = amigaFileSize(base, &size);
    if (sizeResult) return sizeResult;
    if (offset > INT32_MAX || offset < 0 || offset + amount > INT32_MAX)
        return SQLITE_FULL;
    if (offset > size) {
        unsigned char zeros[512] = { 0 };
        int result = seekTo(file, size, 0);
        if (result) return result;
        while (size < offset) {
            LONG chunk = offset - size > sizeof(zeros) ? sizeof(zeros) : offset - size;
            if (Write(file->handle, zeros, chunk) != chunk) return ioError(SQLITE_IOERR_WRITE);
            size += chunk;
        }
    }
    int result = seekTo(file, offset, amount);
    if (result) return result;
    LONG count = Write(file->handle, (APTR)data, amount);
    if (count != amount) return ioError(SQLITE_IOERR_WRITE);
    return SQLITE_OK;
}

static int amigaTruncate(sqlite3_file* base, sqlite3_int64 size)
{
    AmigaFile* file = (AmigaFile*)base;
    if (file->readOnly) return SQLITE_READONLY;
    if (size < 0 || size > INT32_MAX) return SQLITE_FULL;
    if (SetFileSize(file->handle, size, OFFSET_BEGINNING) != size)
        return ioError(SQLITE_IOERR_TRUNCATE);
    return SQLITE_OK;
}

static int amigaSync(sqlite3_file* base, int flags)
{
    (void)flags;
    return Flush(((AmigaFile*)base)->handle) ? SQLITE_OK : ioError(SQLITE_IOERR_FSYNC);
}

static int amigaFileSize(sqlite3_file* base, sqlite3_int64* size)
{
    struct FileInfoBlock* info = AllocDosObject(DOS_FIB, NULL);
    if (!info) return SQLITE_NOMEM;
    int result = ExamineFH(((AmigaFile*)base)->handle, info) ? SQLITE_OK : SQLITE_IOERR_FSTAT;
    if (!result) *size = info->fib_Size;
    FreeDosObject(DOS_FIB, info);
    return result;
}

static int amigaLock(sqlite3_file* base, int level)
{
    AmigaFile* file = (AmigaFile*)base;
    int result;
    if (file->lockLevel >= level) return SQLITE_OK;
    if (!file->shared && !file->exclusive) {
        result = lockRange(file, PENDING_BYTE, 1, REC_SHARED_IMMED);
        if (result) return result;
        result = lockRange(file, SHARED_FIRST, SHARED_SIZE, REC_SHARED_IMMED);
        int release = unlockRange(file, PENDING_BYTE, 1);
        if (result) return result;
        file->shared = 1;
        file->lockLevel = SQLITE_LOCK_SHARED;
        if (release) return release;
    }
    if (level >= SQLITE_LOCK_RESERVED && !file->reserved) {
        result = lockRange(file, RESERVED_BYTE, 1, REC_EXCLUSIVE_IMMED);
        if (result) return result;
        file->reserved = 1;
        file->lockLevel = SQLITE_LOCK_RESERVED;
    }
    if (level >= SQLITE_LOCK_PENDING && !file->pending) {
        result = lockRange(file, PENDING_BYTE, 1, REC_EXCLUSIVE_IMMED);
        if (result) return result;
        file->pending = 1;
        file->lockLevel = SQLITE_LOCK_PENDING;
    }
    if (level == SQLITE_LOCK_EXCLUSIVE && !file->exclusive) {
        result = unlockRange(file, SHARED_FIRST, SHARED_SIZE);
        if (result) return result;
        file->shared = 0;
        result = lockRange(file, SHARED_FIRST, SHARED_SIZE, REC_EXCLUSIVE_IMMED);
        if (result) {
            int restore = lockRange(file, SHARED_FIRST, SHARED_SIZE, REC_SHARED_IMMED);
            if (!restore) file->shared = 1;
            return restore ? restore : result;
        }
        file->exclusive = 1;
        file->lockLevel = SQLITE_LOCK_EXCLUSIVE;
    }
    return SQLITE_OK;
}

static int amigaReserved(sqlite3_file* base, int* reserved)
{
    AmigaFile* file = (AmigaFile*)base;
    if (file->reserved) { *reserved = 1; return SQLITE_OK; }
    int result = lockRange(file, RESERVED_BYTE, 1, REC_EXCLUSIVE_IMMED);
    if (!result) {
        *reserved = 0;
        return unlockRange(file, RESERVED_BYTE, 1);
    }
    if (result == SQLITE_BUSY) { *reserved = 1; return SQLITE_OK; }
    return result;
}

static int amigaControl(sqlite3_file* base, int operation, void* argument)
{
    if (operation == SQLITE_FCNTL_LOCKSTATE) {
        *(int*)argument = ((AmigaFile*)base)->lockLevel;
        return SQLITE_OK;
    }
    if (operation == SQLITE_FCNTL_VFSNAME) {
        *(char**)argument = sqlite3_mprintf("amigaos3");
        return *(char**)argument ? SQLITE_OK : SQLITE_NOMEM;
    }
    return SQLITE_NOTFOUND;
}
static int amigaSector(sqlite3_file* file) { (void)file; return 512; }
static int amigaCharacteristics(sqlite3_file* file) { (void)file; return 0; }

static const sqlite3_io_methods methods = {
    1, amigaClose, amigaRead, amigaWrite, amigaTruncate, amigaSync, amigaFileSize,
    amigaLock, amigaUnlock, amigaReserved, amigaControl, amigaSector, amigaCharacteristics
};

static int amigaOpen(sqlite3_vfs* vfs, const char* path, sqlite3_file* base, int flags, int* outFlags)
{
    (void)vfs;
    AmigaFile* file = (AmigaFile*)base;
    char temporary[80], guard[MAX_PATH + 32];
    BPTR guardLock = 0;
    memset(file, 0, sizeof(*file));
    if (!path) {
        unsigned random[4];
        if (!odysseyAmigaOS3RandomBytes((unsigned char*)random, sizeof(random)))
            return SQLITE_CANTOPEN;
        snprintf(temporary, sizeof(temporary), "T:sqlite-%08x%08x%08x%08x", random[0], random[1], random[2], random[3]);
        path = temporary;
        flags |= SQLITE_OPEN_CREATE | SQLITE_OPEN_EXCLUSIVE | SQLITE_OPEN_DELETEONCLOSE;
    }
    if (strlen(path) >= sizeof(file->path)) return SQLITE_CANTOPEN;
    strcpy(file->path, path);
    if (flags & SQLITE_OPEN_CREATE) {
        /* CreateDir is atomic across DOS processes. All CREATE opens in
         * this VFS use the guard, including non-exclusive opens. A stale
         * guard is never guessed safe to remove; report bounded BUSY. */
        snprintf(guard, sizeof(guard), "%s.sqlite-create-lock", path);
        guardLock = CreateDir(guard);
        if (!guardLock)
            return IoErr() == ERROR_OBJECT_EXISTS ? SQLITE_BUSY : ioError(SQLITE_CANTOPEN);
        if (flags & SQLITE_OPEN_EXCLUSIVE) {
            BPTR existing = Lock(path, SHARED_LOCK);
            LONG error = IoErr();
            if (existing) UnLock(existing);
            if (existing || error != ERROR_OBJECT_NOT_FOUND) {
                UnLock(guardLock); DeleteFile(guard);
                return SQLITE_CANTOPEN;
            }
        }
    }
    file->handle = Open(path, flags & SQLITE_OPEN_CREATE ? MODE_READWRITE : MODE_OLDFILE);
    LONG error = IoErr();
    if (guardLock) { UnLock(guardLock); DeleteFile(guard); }
    if (!file->handle) { SetIoErr(error); return ioError(SQLITE_CANTOPEN); }
    file->readOnly = !!(flags & SQLITE_OPEN_READONLY);
    file->deleteOnClose = !!(flags & SQLITE_OPEN_DELETEONCLOSE);
    file->lockLevel = SQLITE_LOCK_NONE;
    base->pMethods = &methods;
    if (outFlags) *outFlags = flags;
    return SQLITE_OK;
}

static int amigaDelete(sqlite3_vfs* vfs, const char* path, int syncDirectory)
{
    (void)vfs;
    char parent[MAX_PATH];
    size_t length = (const char*)FilePart(path) - path;
    if (length >= sizeof(parent)) return SQLITE_IOERR_DELETE;
    memcpy(parent, path, length); parent[length] = 0;
    BPTR lock = syncDirectory ? Lock(parent, SHARED_LOCK) : 0;
    if (syncDirectory && !lock) return SQLITE_IOERR_DIR_FSYNC;
    if (!DeleteFile(path)) {
        LONG error = IoErr();
        if (lock) UnLock(lock);
        return error == ERROR_OBJECT_NOT_FOUND ? SQLITE_OK : SQLITE_IOERR_DELETE;
    }
    int result = SQLITE_OK;
    if (lock) {
        struct MsgPort* handler = ((struct FileLock*)BADDR(lock))->fl_Task;
        if (!DoPkt(handler, ACTION_FLUSH, 0, 0, 0, 0, 0)) result = SQLITE_IOERR_DIR_FSYNC;
        UnLock(lock);
    }
    return result;
}

static int amigaAccess(sqlite3_vfs* vfs, const char* path, int flags, int* available)
{
    (void)vfs;
    *available = 0;
    BPTR lock = Lock(path, SHARED_LOCK);
    if (!lock) return IoErr() == ERROR_OBJECT_NOT_FOUND ? SQLITE_OK : SQLITE_IOERR_ACCESS;
    struct FileInfoBlock* info = AllocDosObject(DOS_FIB, NULL);
    if (!info) { UnLock(lock); return SQLITE_NOMEM; }
    int result = Examine(lock, info) ? SQLITE_OK : SQLITE_IOERR_ACCESS;
    if (!result)
        *available = flags == SQLITE_ACCESS_EXISTS
            || (!(info->fib_Protection & FIBF_READ)
                && (flags != SQLITE_ACCESS_READWRITE || !(info->fib_Protection & FIBF_WRITE)));
    FreeDosObject(DOS_FIB, info); UnLock(lock);
    return result;
}

static int amigaFullPath(sqlite3_vfs* vfs, const char* path, int size, char* output)
{
    (void)vfs;
    if (strchr(path, ':')) {
        if (strlen(path) >= (size_t)size) return SQLITE_CANTOPEN;
        strcpy(output, path);
    } else if (!GetCurrentDirName(output, size) || !AddPart(output, path, size))
        return SQLITE_CANTOPEN;
    return SQLITE_OK;
}
static int amigaRandom(sqlite3_vfs* vfs, int size, char* output)
{
    (void)vfs;
    return odysseyAmigaOS3RandomBytes((unsigned char*)output, size) ? size : 0;
}
static int amigaSleep(sqlite3_vfs* vfs, int microseconds)
{
    (void)vfs;
    ULONG ticks = (microseconds + 19999) / 20000;
    Delay(ticks);
    return ticks * 20000;
}
static int amigaTime64(sqlite3_vfs* vfs, sqlite3_int64* value)
{
    (void)vfs;
    struct timeval now;
    if (gettimeofday(&now, NULL)) return SQLITE_ERROR;
    *value = 210866760000000LL + (sqlite3_int64)now.tv_sec * 1000 + now.tv_usec / 1000;
    return SQLITE_OK;
}
static int amigaTime(sqlite3_vfs* vfs, double* value)
{
    sqlite3_int64 now;
    int result = amigaTime64(vfs, &now);
    if (!result) *value = now / 86400000.0;
    return result;
}

static sqlite3_vfs vfs = {
    2, sizeof(AmigaFile), MAX_PATH, NULL, "amigaos3", NULL,
    amigaOpen, amigaDelete, amigaAccess, amigaFullPath,
    NULL, NULL, NULL, NULL, amigaRandom, amigaSleep, amigaTime, NULL, amigaTime64
};
int sqlite3_os_init(void) { return sqlite3_vfs_register(&vfs, 1); }
int sqlite3_os_end(void) { return SQLITE_OK; }
