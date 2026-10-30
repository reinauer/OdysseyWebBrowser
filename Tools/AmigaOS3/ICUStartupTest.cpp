/* Run the browser's actual static-data startup gate with a real MUI base. */
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <unicode/uclean.h>
#include <unicode/icudataver.h>
#include <unicode/uversion.h>
#include <cstdio>

struct Library* MUIMasterBase;
extern ULONG icu_check(void);

int main()
{
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) {
        std::puts("FAIL opening MUI for ICU startup check");
        return 1;
    }
    unsigned failures = !icu_check();
    // Calling the startup gate twice must not invalidate initialized ICU.
    failures += !icu_check();
    UErrorCode status = U_ZERO_ERROR;
    UVersionInfo version;
    char text[U_MAX_VERSION_STRING_LENGTH];
    u_getDataVersion(version, &status);
    u_versionToString(version, text);
    std::printf("Actual linked ICU data version: %s\n", text);
    failures += U_FAILURE(status);
    u_cleanup();
    CloseLibrary(MUIMasterBase);
    std::printf("ICU startup: %u failures\n", failures);
    return failures ? 1 : 0;
}
