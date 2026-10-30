/* Missing public descriptor APIs used by curl's command-line frontend. */
#define _DEFAULT_SOURCE 1
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <dos/dos.h>
#include <proto/dos.h>

/* Public checked accessor exported by the selected SDK's newlib port. */
extern BPTR __check_fno(unsigned);

static int nativeFileError(void)
{
    switch (IoErr()) {
    case ERROR_DISK_FULL: errno = ENOSPC; break;
    case ERROR_WRITE_PROTECTED: errno = EACCES; break;
    case ERROR_DISK_WRITE_PROTECTED: errno = EROFS; break;
    case ERROR_OBJECT_WRONG_TYPE: errno = EINVAL; break;
    case ERROR_ACTION_NOT_KNOWN: errno = ENOTSUP; break;
    default: errno = EIO; break;
    }
    return -1;
}

int ftruncate(int fd, off_t length)
{
    BPTR handle = __check_fno(fd);
    if (!handle) {
        errno = EBADF;
        return -1;
    }
    if (length < 0 || length > LONG_MAX) {
        errno = EINVAL;
        return -1;
    }
    LONG position = Seek(handle, 0, OFFSET_CURRENT);
    if (position == -1)
        return nativeFileError();
    /* DOS cannot preserve a descriptor position beyond the new EOF. Refuse
     * before modifying the file instead of silently changing its offset. */
    if (position > length) {
        errno = ENOTSUP;
        return -1;
    }
    struct stat info;
    if (fstat(fd, &info))
        return -1;
    if (length > info.st_size) {
        /* SetFileSize leaves newly allocated bytes uninitialized. Extend
         * with explicit zeros instead, never exposing old disk contents. */
        static const char zeros[512] = { 0 };
        if (Seek(handle, 0, OFFSET_END) == -1)
            return nativeFileError();
        off_t remaining = length - info.st_size;
        while (remaining) {
            LONG count = remaining > (off_t)sizeof(zeros) ? sizeof(zeros) : remaining;
            LONG written = Write(handle, zeros, count);
            if (written <= 0) {
                nativeFileError();
                int savedError = errno;
                SetFileSize(handle, info.st_size, OFFSET_BEGINNING);
                Seek(handle, position, OFFSET_BEGINNING);
                errno = savedError;
                return -1;
            }
            remaining -= written;
        }
        return Seek(handle, position, OFFSET_BEGINNING) == -1 ? nativeFileError() : 0;
    }
    LONG actual = SetFileSize(handle, length, OFFSET_BEGINNING);
    if (actual == -1)
        return nativeFileError();
    if (Seek(handle, position, OFFSET_BEGINNING) == -1)
        return nativeFileError();
    if (actual != length) {
        errno = EIO;
        return -1;
    }
    return 0;
}
