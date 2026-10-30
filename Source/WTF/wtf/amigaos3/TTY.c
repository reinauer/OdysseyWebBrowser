/* SDK isatty only classifies descriptor numbers, including redirected stdio.
 * Link this object explicitly with --wrap=isatty to test the actual handle. */
#define _DEFAULT_SOURCE 1
#include <errno.h>
#include <dos/dos.h>
#include <proto/dos.h>

/* Checked descriptor accessor exported by the native newlib port. */
extern BPTR __check_fno(unsigned);

int __wrap_isatty(int fd)
{
    BPTR handle = __check_fno(fd);
    if (!handle) {
        errno = EBADF;
        return 0;
    }
    if (IsInteractive(handle))
        return 1;
    errno = ENOTTY;
    return 0;
}
