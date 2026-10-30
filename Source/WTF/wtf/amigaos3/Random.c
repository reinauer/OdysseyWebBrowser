/* AmiSSL's random generator supplies cryptographic entropy on classic OS. */
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <libraries/amisslmaster.h>
#include <proto/amissl.h>
#include <proto/amisslmaster.h>
#include <proto/exec.h>

int odysseyAmigaOS3RandomBytes(unsigned char* buffer, size_t length)
{
    /* Open a task-local AmiSSL context. Its library base must not be shared
       by independent SDK pthread tasks, even when calls are serialized. */
    struct Library* AmiSSLMasterBase = OpenLibrary("amisslmaster.library",
        AMISSLMASTER_MIN_VERSION);
    if (!AmiSSLMasterBase)
        return 0;
    struct Library* AmiSSLBase = NULL;
    struct Library* AmiSSLExtBase = NULL;
    int localError = 0;
    struct TagItem tags[] = {
        { AmiSSL_UsesOpenSSLStructs, FALSE },
        { AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase },
        { AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase },
        { AmiSSL_ErrNoPtr, (ULONG)&localError },
        { TAG_DONE, 0 }
    };
    int success = 0;
    if (!OpenAmiSSLTagList(AMISSL_CURRENT_VERSION, tags)) {
        success = 1;
        while (length) {
            int chunk = length > INT_MAX ? INT_MAX : (int)length;
            if (RAND_bytes(buffer, chunk) != 1) {
                success = 0;
                break;
            }
            buffer += chunk;
            length -= chunk;
        }
        CloseAmiSSL();
    }
    CloseLibrary(AmiSSLMasterBase);
    return success;
}
