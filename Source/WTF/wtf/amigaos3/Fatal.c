/* WebKit's CRASH() is abort() in release builds, and failed allocations end
 * there too. The SDK's abort() leaves through _exit(), which skips the
 * atexit() handlers that stop curl and the work queues; the SDK's exit code
 * then joins those threads forever, and the program stops without a word.
 * The linker routes abort() here (--wrap=abort), so that the program's
 * handler can report the error and quit. */
#include "Fatal.h"
#include "Malloc.h"

#include <stdio.h>
#include <unistd.h>

extern char _stext[]; /* start of the code hunk */

static const char* volatile fatalReason;
static AmigaOS3FatalHandler volatile fatalHandler;

void amigaos3SetFatalHandler(AmigaOS3FatalHandler handler)
{
    fatalHandler = handler;
}

void amigaos3SetFatalReason(const char* reason)
{
    fatalReason = reason;
}

void __attribute__((noreturn)) __wrap_abort(void)
{
    /* abort() does not return, so its call may be the caller's last
     * instruction; step back into the call so that nm names the caller. */
    unsigned long offset = (unsigned long)__builtin_return_address(0) - 2 - (unsigned long)_stext;
    /* The reserve lets the report and the requester allocate. */
    amigaos3ReleaseReserve();
    const char* reason = fatalReason;
    if (reason)
        fprintf(stderr, "OWB: fatal error: %s\n", reason);
    else
        fprintf(stderr, "OWB: fatal error at code offset 0x%lx\n", offset);
    AmigaOS3FatalHandler handler = fatalHandler;
    if (handler)
        handler(reason, offset);
    /* As the SDK's abort() does, leave through _exit(), which runs the exit
     * functions but not atexit(), with AmigaDOS's failure code instead of
     * 127. */
    _exit(20);
}
