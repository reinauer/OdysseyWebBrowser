/* Fatal errors on AmigaOS 3: the program's handler for abort(). */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Called in the task that failed, before the program exits with 20, for every
 * call of abort(), including one from another task or from the handler
 * itself. reason is a phrase such as "ran out of memory", or NULL for an
 * internal error; codeOffset lies in the instruction that called abort(),
 * counted from the start of the code hunk, for use with nm. A handler may end
 * the task or leave by longjmp() instead of returning. */
typedef void (*AmigaOS3FatalHandler)(const char* reason, unsigned long codeOffset);

void amigaos3SetFatalHandler(AmigaOS3FatalHandler handler);

/* Record why the next abort() happens; reason must stay valid. */
void amigaos3SetFatalReason(const char* reason);

#ifdef __cplusplus
}
#endif
