/* Import the native process environment before C++ static initialization.
 * Newlib owns copies made by setenv; no pointer into DOS or temporary buffers
 * escapes this startup snapshot. Names retain their spelling and getenv
 * remains POSIX case-sensitive. Same-spelling local values replace globals.
 * Later setenv/unsetenv retain POSIX behavior.
 */
#include <dos/dosextens.h>
#include <dos/var.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stabs.h>

static int isText(const unsigned char* bytes, ULONG size)
{
    for (ULONG i = 0; i < size; ++i) {
        unsigned char c = bytes[i];
        if ((!c || c < 32 || c == 127) && c != '\t' && c != '\n' && c != '\r')
            return 0;
    }
    return 1;
}

static void importValue(const char* name, ULONG size, ULONG flags)
{
    if (!name || !*name || strchr(name, '=') || size >= LONG_MAX)
        return;
    char* value = malloc(size + 1);
    if (!value)
        return;
    LONG length = GetVar(name, value, size + 1, flags);
    if (length >= 0 && (ULONG)length <= size) {
        value[length] = 0;
        setenv(name, value, 1);
    }
    free(value);
}

static void importGlobals(void)
{
    BPTR lock = Lock("ENV:", SHARED_LOCK);
    if (!lock)
        return;
    struct FileInfoBlock* info = AllocDosObject(DOS_FIB, NULL);
    if (info && Examine(lock, info)) {
        while (ExNext(lock, info)) {
            /* Subdirectories hold OS/application preferences, not flat
             * process environment names. Never follow directory links. */
            if (info->fib_DirEntryType >= 0 || info->fib_Size < 0
                || info->fib_Size == LONG_MAX)
                continue;
            ULONG size = info->fib_Size;
            char* path = malloc(strlen(info->fib_FileName) + 5);
            unsigned char* bytes = malloc(size + 1);
            if (path && bytes) {
                strcpy(path, "ENV:");
                strcat(path, info->fib_FileName);
                BPTR file = Open(path, MODE_OLDFILE);
                if (file) {
                    LONG length = Read(file, bytes, size + 1);
                    Close(file);
                    /* Reject binary preference files and concurrent size
                     * changes before applying GetVar's text semantics. */
                    if (length == (LONG)size && isText(bytes, size))
                        importValue(info->fib_FileName, size, GVF_GLOBAL_ONLY);
                }
            }
            free(bytes);
            free(path);
        }
    }
    if (info)
        FreeDosObject(DOS_FIB, info);
    UnLock(lock);
}

void odysseyAmigaOS3InitializeEnvironment(void)
{
    int savedErrno = errno;
    LONG savedIoErr = IoErr();
    importGlobals();
    struct Process* process = (struct Process*)FindTask(NULL);
    if (process->pr_Task.tc_Node.ln_Type == NT_PROCESS) {
        struct LocalVar* variable = (struct LocalVar*)process->pr_LocalVars.mlh_Head;
        for (; variable->lv_Node.ln_Succ;
             variable = (struct LocalVar*)variable->lv_Node.ln_Succ) {
            if (variable->lv_Node.ln_Type == LV_VAR
                && !(variable->lv_Flags & GVF_BINARY_VAR)
                && isText((unsigned char*)variable->lv_Value, variable->lv_Len))
                importValue(variable->lv_Node.ln_Name, variable->lv_Len, GVF_LOCAL_ONLY);
        }
    }
    SetIoErr(savedIoErr);
    errno = savedErrno;
}

/* SDK libraries: -100; pthread setup: -6; C++ constructors: +100.
 * The SDK heap and our recursive heap wrapper initialize lazily. */
ADD2INIT(odysseyAmigaOS3InitializeEnvironment, 0);
