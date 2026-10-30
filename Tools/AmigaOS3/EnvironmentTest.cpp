/* Exercise the production startup importer before real C++ constructors. */
#include <dos/var.h>
#include <proto/dos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stabs.h>

static unsigned setupFailures, constructorFailures, binaryLocalFlags;
static char longValue[4097];

extern "C" void prepareNativeEnvironment(void)
{
    for (unsigned i = 0; i < sizeof(longValue) - 1; ++i)
        longValue[i] = 'a' + i % 26;
    const char* names[] = { "ODYSSEY_ENV_GLOBAL", "ODYSSEY_ENV_OVERRIDE",
        "ODYSSEY_ENV_LOCAL", "ODYSSEY_ENV_EMPTY", "ODYSSEY_ENV_LONG",
        "ODYSSEY_ENV_BINARY", "ODYSSEY_ENV_LOCAL_BINARY",
        "ODYSSEY_ENV_ALIAS", "ODYSSEY_ENV_IGNORED", NULL };
    for (unsigned i = 0; names[i]; ++i)
        unsetenv(names[i]);
    setupFailures += !SetVar("ODYSSEY_ENV_GLOBAL", "space = tab\tUTF8 \xce\xa9", -1, GVF_GLOBAL_ONLY);
    setupFailures += !SetVar("ODYSSEY_ENV_OVERRIDE", "global", -1, GVF_GLOBAL_ONLY);
    setupFailures += !SetVar("ODYSSEY_ENV_OVERRIDE", "local wins = yes", -1, GVF_LOCAL_ONLY);
    setupFailures += !SetVar("ODYSSEY_ENV_LOCAL", "local only", -1, GVF_LOCAL_ONLY);
    setupFailures += !SetVar("ODYSSEY_ENV_EMPTY", "", 0, GVF_GLOBAL_ONLY);
    setupFailures += !SetVar("ODYSSEY_ENV_LONG", longValue, -1, GVF_GLOBAL_ONLY);
    const char binary[] = { 'F', 'O', 'R', 'M', 0, 1, 2, 3 };
    setupFailures += !SetVar("ODYSSEY_ENV_BINARY", binary, sizeof(binary), GVF_GLOBAL_ONLY | GVF_BINARY_VAR);
    setupFailures += !SetVar("ODYSSEY_ENV_LOCAL_BINARY", "ASCII binary", -1, GVF_LOCAL_ONLY | GVF_BINARY_VAR);
    struct LocalVar* binaryLocal = FindVar("ODYSSEY_ENV_LOCAL_BINARY", LV_VAR);
    if (binaryLocal)
        binaryLocalFlags = binaryLocal->lv_Flags;
    else
        ++setupFailures;
    setupFailures += !SetVar("ODYSSEY_ENV_ALIAS", "not an environment variable", -1, GVF_LOCAL_ONLY | LV_ALIAS);
    setupFailures += !SetVar("ODYSSEY_ENV_IGNORED", "ignored", -1, GVF_LOCAL_ONLY);
    struct LocalVar* ignored = FindVar("ODYSSEY_ENV_IGNORED", LV_VAR);
    if (ignored)
        ignored->lv_Node.ln_Type |= LVF_IGNORE;
    else
        ++setupFailures;
}
ADD2INIT(prepareNativeEnvironment, -1);

static bool equals(const char* name, const char* value)
{
    const char* actual = getenv(name);
    return actual && !strcmp(actual, value);
}
static unsigned checkValues(void)
{
    return !equals("ODYSSEY_ENV_GLOBAL", "space = tab\tUTF8 \xce\xa9")
        + !equals("ODYSSEY_ENV_OVERRIDE", "local wins = yes")
        + !equals("ODYSSEY_ENV_LOCAL", "local only")
        + !equals("ODYSSEY_ENV_EMPTY", "")
        + !equals("ODYSSEY_ENV_LONG", longValue)
        + !!getenv("ODYSSEY_ENV_BINARY")
        + !!getenv("ODYSSEY_ENV_LOCAL_BINARY")
        + !!getenv("ODYSSEY_ENV_ALIAS")
        + !!getenv("ODYSSEY_ENV_IGNORED");
}
struct StaticReader {
    StaticReader() { constructorFailures = checkValues(); }
};
static StaticReader staticReader;

int main()
{
    unsigned failures = setupFailures + constructorFailures;
    printf("Native ENV setup failures: %u\n", setupFailures);
    printf("Native binary local flags: %04x\n", binaryLocalFlags);
    printf("C++ constructor getenv failures: %u\n", constructorFailures);
    for (unsigned i = 0; i < 100; ++i) {
        char* scratch = (char*)malloc(8192);
        if (!scratch) { ++failures; break; }
        memset(scratch, 0xa5, 8192);
        free(scratch);
    }
    unsigned lifetimeFailures = checkValues();
    failures += lifetimeFailures;
    printf("Imported value lifetime failures: %u\n", lifetimeFailures);
    failures += setenv("ODYSSEY_ENV_OVERRIDE", "POSIX replacement", 1) != 0;
    failures += !equals("ODYSSEY_ENV_OVERRIDE", "POSIX replacement");
    failures += unsetenv("ODYSSEY_ENV_LOCAL") != 0 || getenv("ODYSSEY_ENV_LOCAL") != NULL;
    printf("Native environment: %u failures\n", failures);
    return failures ? 20 : 0;
}
