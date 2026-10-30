/* Use AmiSSL's documented owner/subprocess initialization API explicitly.
 * The SDK auto archive uses constructors that newlib startup does not run.
 * CurlRequestScheduler has one worker; SocketBase belongs to that worker
 * while it is running. The owner must join it before closing AmiSSL. */
#include "AmigaOS3AmiSSL.h"
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <libraries/amisslmaster.h>
#include <proto/exec.h>
#include <proto/bsdsocket.h>
#include <proto/amissl.h>
#include <proto/amisslmaster.h>

struct Library* AmiSSLBase;
struct Library* AmiSSLExtBase;
struct Library* AmiSSLMasterBase;
struct Library* SocketBase;
static struct Library* ownerSocketBase;
static struct Task* ownerTask;
static struct Task* workerTask;
static int ownerSocketError;
static int workerSocketError;

int* odysseyAmigaOS3SocketError(void)
{
    struct Task* current = FindTask(NULL);
    if (current == workerTask)
        return &workerSocketError;
    if (current == ownerTask)
        return &ownerSocketError;
    /* Native socket handles and their error channels belong to these two
       initialized tasks. A third caller must not borrow either context. */
    abort();
}

int odysseyAmigaOS3SocketReceiveBuffer(int socket, int size)
{
    if (!workerTask || workerTask != FindTask(NULL))
        return -1;
    return setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
}

static int bindSocketError(struct Library* SocketBase, int* socketError)
{
    struct TagItem tags[] = {
        { SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(*socketError))), (ULONG)socketError },
        { TAG_DONE, 0 }
    };
    return !SocketBaseTagList(tags);
}

int odysseyAmigaOS3AmiSSLInitialize(void)
{
    if (ownerTask)
        return ownerTask == FindTask(NULL);
    ownerSocketBase = OpenLibrary("bsdsocket.library", 4);
    AmiSSLMasterBase = OpenLibrary("amisslmaster.library", AMISSLMASTER_MIN_VERSION);
    ownerSocketError = 0;
    if (!ownerSocketBase || !AmiSSLMasterBase || !bindSocketError(ownerSocketBase, &ownerSocketError))
        goto fail;
    struct TagItem tags[] = {
        { AmiSSL_UsesOpenSSLStructs, TRUE },
        { AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase },
        { AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase },
        { AmiSSL_SocketBase, (ULONG)ownerSocketBase },
        { AmiSSL_ErrNoPtr, (ULONG)&ownerSocketError },
        { TAG_DONE, 0 }
    };
    if (OpenAmiSSLTagList(AMISSL_CURRENT_VERSION, tags))
        goto fail;
    SocketBase = ownerSocketBase;
    ownerTask = FindTask(NULL);
    return 1;
fail:
    if (AmiSSLMasterBase)
        CloseLibrary(AmiSSLMasterBase);
    if (ownerSocketBase)
        CloseLibrary(ownerSocketBase);
    AmiSSLMasterBase = ownerSocketBase = NULL;
    return 0;
}

int odysseyAmigaOS3AmiSSLWorkerInitialize(void)
{
    if (!ownerTask || workerTask || FindTask(NULL) == ownerTask)
        return 0;
    struct Library* socket = OpenLibrary("bsdsocket.library", 4);
    if (!socket)
        return 0;
    workerSocketError = 0;
    struct TagItem tags[] = {
        { AmiSSL_SocketBase, (ULONG)socket },
        { AmiSSL_ErrNoPtr, (ULONG)&workerSocketError },
        { TAG_DONE, 0 }
    };
    if (!bindSocketError(socket, &workerSocketError) || InitAmiSSLA(tags)) {
        CloseLibrary(socket);
        return 0;
    }
    workerTask = FindTask(NULL);
    SocketBase = socket;
    return 1;
}

void odysseyAmigaOS3AmiSSLWorkerCleanup(void)
{
    if (FindTask(NULL) != workerTask)
        return;
    struct TagItem tags[] = { { TAG_DONE, 0 } };
    CleanupAmiSSLA(tags);
    CloseLibrary(SocketBase);
    SocketBase = ownerSocketBase;
    workerTask = NULL;
}

void odysseyAmigaOS3AmiSSLCleanup(void)
{
    if (FindTask(NULL) != ownerTask || workerTask)
        return;
    CloseAmiSSL();
    CloseLibrary(AmiSSLMasterBase);
    CloseLibrary(ownerSocketBase);
    AmiSSLBase = AmiSSLExtBase = AmiSSLMasterBase = SocketBase = ownerSocketBase = NULL;
    ownerTask = NULL;
}
