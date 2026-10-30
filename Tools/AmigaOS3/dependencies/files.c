/* File operations needed by fontconfig, implemented with AmigaDOS. */
#define _DEFAULT_SOURCE 1
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/record.h>
#include <proto/dos.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <reent.h>
#include <limits.h>

/* AmigaPorts/newlib-cygwin amiga, libc/sys/amigaos/open.c exports this
 * same checked handle accessor used by its lseek.c. No table layout used. */
extern BPTR __check_fno(unsigned);

static int fileError(void)
{
    switch (IoErr()) {
    case ERROR_OBJECT_NOT_FOUND: case ERROR_DIR_NOT_FOUND: errno = ENOENT; break;
    case ERROR_OBJECT_EXISTS: errno = EEXIST; break;
    case ERROR_DISK_WRITE_PROTECTED: errno = EROFS; break;
    case ERROR_READ_PROTECTED: case ERROR_WRITE_PROTECTED:
    case ERROR_DELETE_PROTECTED: errno = EACCES; break;
    case ERROR_OBJECT_IN_USE: errno = EBUSY; break;
    case ERROR_DIRECTORY_NOT_EMPTY: errno = ENOTEMPTY; break;
    case ERROR_NO_FREE_STORE: errno = ENOMEM; break;
    case ERROR_DISK_FULL: errno = ENOSPC; break;
    case ERROR_LOCK_COLLISION: case ERROR_LOCK_TIMEOUT: errno = EAGAIN; break;
    case ERROR_OBJECT_WRONG_TYPE: errno = ENOTDIR; break;
    default: errno = EIO; break;
    }
    return -1;
}

static int fillStat(struct FileInfoBlock* info, struct stat* value)
{
    struct timezone zone;
    const char* dst = getenv("DST_OFFSET");
    if (gettimeofday(NULL, &zone))
        return -1;
    memset(value, 0, sizeof(*value));
    value->st_ino = info->fib_DiskKey;
    value->st_mode = info->fib_DirEntryType > 0 ? S_IFDIR : S_IFREG;
    if (!(info->fib_Protection & FIBF_READ)) value->st_mode |= 0444;
    if (!(info->fib_Protection & FIBF_WRITE)) value->st_mode |= 0222;
    if (!(info->fib_Protection & FIBF_EXECUTE)) value->st_mode |= 0111;
    value->st_size = info->fib_Size;
    value->st_nlink = 1;
    value->st_blksize = 512;
    value->st_blocks = info->fib_NumBlocks;
    value->st_mtime = info->fib_Date.ds_Days * 86400L
        + info->fib_Date.ds_Minute * 60L + info->fib_Date.ds_Tick / TICKS_PER_SECOND
        + 252460800L + zone.tz_minuteswest * 60 + (dst ? strtol(dst, NULL, 10) : 0);
    value->st_atime = value->st_ctime = value->st_mtime;
    return 0;
}

int stat(const char* path, struct stat* value)
{
    BPTR lock = Lock(path, SHARED_LOCK);
    int examineParent = !lock && IoErr() == ERROR_OBJECT_IN_USE;
    if (!lock && !examineParent)
        return fileError();
    const char* name = FilePart(path);
    if (examineParent) {
        /* MODE_NEWFILE holds an exclusive lock. Enumerating its parent
         * obtains metadata without acquiring a conflicting file lock. */
        size_t length = name - path;
        char* parent = malloc(length + 1);
        if (!parent) return -1;
        memcpy(parent, path, length);
        parent[length] = 0;
        lock = Lock(parent, SHARED_LOCK);
        free(parent);
        if (!lock) return fileError();
    }
    struct FileInfoBlock* info = AllocDosObject(DOS_FIB, NULL);
    int result;
    if (!info) {
        UnLock(lock);
        errno = ENOMEM;
        return -1;
    }
    if (!Examine(lock, info))
        result = fileError();
    else if (examineParent) {
        result = -1;
        while (ExNext(lock, info)) {
            if (!strcasecmp(name, info->fib_FileName)) {
                result = fillStat(info, value);
                break;
            }
        }
        if (result) {
            if (IoErr() == ERROR_NO_MORE_ENTRIES) errno = ENOENT;
            else fileError();
        }
    } else
        result = fillStat(info, value);
    if (!result)
        value->st_dev = ((struct FileLock*)BADDR(lock))->fl_Volume;
    FreeDosObject(DOS_FIB, info);
    UnLock(lock);
    return result;
}

int fstat(int fd, struct stat* value)
{
    BPTR handle = __check_fno(fd);
    if (!handle) {
        errno = EBADF;
        return -1;
    }
    struct FileInfoBlock* info = AllocDosObject(DOS_FIB, NULL);
    if (!info) {
        errno = ENOMEM;
        return -1;
    }
    int result = ExamineFH(handle, info) ? fillStat(info, value) : fileError();
    if (!result) {
        BPTR lock = DupLockFromFH(handle);
        if (lock) {
            value->st_dev = ((struct FileLock*)BADDR(lock))->fl_Volume;
            UnLock(lock);
        }
    }
    FreeDosObject(DOS_FIB, info);
    return result;
}

/* Newlib's stdio uses the reentrant entry point, bypassing public fstat.
 * Override that entry too, without pulling its dummy _fstat implementation. */
int _fstat_r(struct _reent* reent, int fd, struct stat* value)
{
    int result = fstat(fd, value);
    if (result) reent->_errno = errno;
    return result;
}

int _stat_r(struct _reent* reent, const char* path, struct stat* value)
{
    int result = stat(path, value);
    if (result) reent->_errno = errno;
    return result;
}

int chmod(const char* path, mode_t mode)
{
    BPTR lock = Lock(path, SHARED_LOCK);
    if (!lock) return fileError();
    struct FileInfoBlock* info = AllocDosObject(DOS_FIB, NULL);
    if (!info) { UnLock(lock); errno = ENOMEM; return -1; }
    if (!Examine(lock, info)) {
        int result = fileError();
        FreeDosObject(DOS_FIB, info); UnLock(lock); return result;
    }
    LONG protection = info->fib_Protection | FIBF_READ | FIBF_WRITE | FIBF_EXECUTE | FIBF_DELETE;
    FreeDosObject(DOS_FIB, info); UnLock(lock);
    if (mode & S_IRUSR) protection &= ~FIBF_READ;
    if (mode & S_IWUSR) protection &= ~(FIBF_WRITE | FIBF_DELETE);
    if (mode & S_IXUSR) protection &= ~FIBF_EXECUTE;
    return SetProtection(path, protection) ? 0 : fileError();
}

char* realpath(const char* path, char* result)
{
    BPTR lock = Lock(path, SHARED_LOCK);
    if (!lock) { fileError(); return NULL; }
    int allocated = !result;
    if (allocated && !(result = malloc(PATH_MAX))) { UnLock(lock); return NULL; }
    if (!NameFromLock(lock, result, PATH_MAX)) {
        fileError();
        if (allocated) free(result);
        result = NULL;
    }
    UnLock(lock);
    return result;
}

int access(const char* path, int mode)
{
    struct stat value;
    if (mode & ~(R_OK | W_OK | X_OK)) {
        errno = EINVAL;
        return -1;
    }
    if (stat(path, &value))
        return -1;
    if (((mode & R_OK) && !(value.st_mode & 0400))
        || ((mode & W_OK) && !(value.st_mode & 0200))
        || ((mode & X_OK) && !(value.st_mode & 0100))) {
        errno = EACCES;
        return -1;
    }
    return 0;
}

int mkdir(const char* path, mode_t mode)
{
    (void)mode; /* Use the filesystem's default Amiga protection bits. */
    BPTR lock = CreateDir(path);
    if (!lock)
        return fileError();
    UnLock(lock);
    return 0;
}

int rmdir(const char* path)
{
    struct stat value;
    if (stat(path, &value))
        return -1;
    if (!S_ISDIR(value.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    if (!DeleteFile(path))
        return fileError();
    return 0;
}

char* getcwd(char* buffer, size_t size)
{
    int allocated = !buffer;
    if (!size && buffer) {
        errno = EINVAL;
        return NULL;
    }
    if (!size) size = 4096;
    if (!buffer && !(buffer = malloc(size)))
        return NULL;
    if (!GetCurrentDirName(buffer, size)) {
        fileError();
        if (IoErr() == ERROR_LINE_TOO_LONG) errno = ERANGE;
        if (allocated) free(buffer);
        return NULL;
    }
    return buffer;
}

int fcntl(int fd, int command, ...)
{
    BPTR handle = __check_fno(fd);
    if (!handle) {
        errno = EBADF;
        return -1;
    }
    /* Newlib descriptors are process-private; DOS child processes inherit
     * only Input/Output, never this descriptor table. */
    if (command == F_GETFD || command == F_SETFD)
        return 0;
    if (command != F_SETLK && command != F_SETLKW) {
        errno = EINVAL;
        return -1;
    }
    va_list args;
    va_start(args, command);
    struct flock* lock = va_arg(args, struct flock*);
    va_end(args);
    if (!lock || lock->l_whence != SEEK_SET || lock->l_start < 0 || lock->l_len < 0) {
        errno = EINVAL;
        return -1;
    }
    ULONG length = lock->l_len ? lock->l_len : 0xffffffffUL - lock->l_start;
    if (lock->l_type == F_UNLCK)
        return UnLockRecord(handle, lock->l_start, length) ? 0 : fileError();
    if (lock->l_type != F_RDLCK && lock->l_type != F_WRLCK) {
        errno = EINVAL;
        return -1;
    }
    ULONG mode = lock->l_type == F_RDLCK ? REC_SHARED : REC_EXCLUSIVE;
    if (command == F_SETLK)
        mode |= REC_EXCLUSIVE_IMMED;
    return LockRecord(handle, lock->l_start, length, mode, 0x7fffffffUL) ? 0 : fileError();
}

int _link(const char* from, const char* to)
{
    BPTR lock = Lock(from, SHARED_LOCK);
    if (!lock)
        return fileError();
    int result = MakeLink(to, lock, LINK_HARD) ? 0 : fileError();
    UnLock(lock);
    return result;
}

int symlink(const char* target, const char* name)
{
    if (!MakeLink(name, (LONG)target, LINK_SOFT))
        return fileError();
    return 0;
}

int utimes(const char* name, const struct timeval times[2])
{
    struct timeval now;
    struct timezone zone;
    struct DateStamp stamp;
    const char* dst = getenv("DST_OFFSET");
    long seconds;
    if (gettimeofday(&now, &zone))
        return -1;
    if (times)
        now = times[1]; /* AmigaDOS stores modification time only. */
    if (now.tv_usec < 0 || now.tv_usec >= 1000000) {
        errno = EINVAL;
        return -1;
    }
    seconds = now.tv_sec - 252460800L - zone.tz_minuteswest * 60
        - (dst ? strtol(dst, NULL, 10) : 0);
    if (seconds < 0) {
        errno = EINVAL;
        return -1;
    }
    stamp.ds_Days = seconds / 86400;
    stamp.ds_Minute = (seconds % 86400) / 60;
    stamp.ds_Tick = (seconds % 60) * TICKS_PER_SECOND
        + now.tv_usec / (1000000 / TICKS_PER_SECOND);
    if (!SetFileDate(name, &stamp))
        return fileError();
    return 0;
}
