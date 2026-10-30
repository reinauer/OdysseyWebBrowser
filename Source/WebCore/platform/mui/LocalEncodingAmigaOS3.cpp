#include "LocalEncodingAmigaOS3.h"

#include <diskfont/diskfonttag.h>
#include <proto/diskfont.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <unicode/ucnv.h>
#include <unicode/ucnv_err.h>
#include <climits>
#include <cstdlib>
#include <cstring>

namespace WebCore::AmigaOS3 {

bool localEncodingName(char* name, size_t capacity)
{
    if (capacity < 2 || capacity > INT_MAX)
        return false;
    auto length = GetVar("ODYSSEY_LOCAL_CHARSET", name, capacity, 0);
    if (length >= 0)
        return length && static_cast<size_t>(length) < capacity - 1;

    // ObtainCharsetInfo is V47 in the classic SDK's diskfont_lib.sfd.
    // Earlier classic systems have no charset metadata; their default is
    // ISO-8859-1. An explicit environment setting supports other keymaps.
    struct Library* DiskfontBase = OpenLibrary("diskfont.library", 47);
    const char* charset = "ISO-8859-1";
    if (DiskfontBase) {
        ULONG number = GetDiskFontCtrl(DFCTRL_CHARSET);
        const char* systemName = reinterpret_cast<const char*>(ObtainCharsetInfo(DFCS_NUMBER, number, DFCS_MIMENAME));
        if (systemName && *systemName)
            charset = systemName;
    }
    bool fits = std::strlen(charset) < capacity;
    if (fits)
        std::strcpy(name, charset);
    if (DiskfontBase)
        CloseLibrary(DiskfontBase);
    return fits;
}

static char* convert(const char* bytes, size_t length, const char* from, const char* to, size_t* convertedLength = nullptr)
{
    if (convertedLength)
        *convertedLength = 0;
    if ((!bytes && length) || length > INT_MAX)
        return nullptr;
    UErrorCode error = U_ZERO_ERROR;
    UConverter* source = ucnv_open(from, &error);
    UConverter* destination = U_SUCCESS(error) ? ucnv_open(to, &error) : nullptr;
    UChar* unicode = nullptr;
    char* result = nullptr;
    if (U_SUCCESS(error)) {
        ucnv_setToUCallBack(source, UCNV_TO_U_CALLBACK_STOP, nullptr, nullptr, nullptr, &error);
        int32_t required = ucnv_toUChars(source, nullptr, 0, bytes, length, &error);
        if ((error == U_BUFFER_OVERFLOW_ERROR || U_SUCCESS(error)) && required < INT_MAX) {
            unicode = static_cast<UChar*>(std::malloc((static_cast<size_t>(required) + 1) * sizeof(UChar)));
            if (unicode) {
                error = U_ZERO_ERROR;
                required = ucnv_toUChars(source, unicode, required + 1, bytes, length, &error);
                if (U_SUCCESS(error)) {
                    int32_t outputLength = ucnv_fromUChars(destination, nullptr, 0, unicode, required, &error);
                    if ((error == U_BUFFER_OVERFLOW_ERROR || U_SUCCESS(error)) && outputLength < INT_MAX) {
                        result = static_cast<char*>(std::malloc(static_cast<size_t>(outputLength) + 1));
                        if (result) {
                            error = U_ZERO_ERROR;
                            ucnv_fromUChars(destination, result, outputLength + 1, unicode, required, &error);
                            if (U_FAILURE(error)) {
                                std::free(result);
                                result = nullptr;
                            } else if (convertedLength)
                                *convertedLength = outputLength;
                        }
                    }
                }
            }
        }
    }
    std::free(unicode);
    if (source)
        ucnv_close(source);
    if (destination)
        ucnv_close(destination);
    return result;
}

char* localToUTF8(const char* bytes, size_t length, size_t* outputLength)
{
    if (outputLength)
        *outputLength = 0;
    char encoding[80];
    return localEncodingName(encoding, sizeof(encoding)) ? convert(bytes, length, encoding, "UTF-8", outputLength) : nullptr;
}

char* utf8ToLocal(const char* bytes, size_t length)
{
    char encoding[80];
    return localEncodingName(encoding, sizeof(encoding)) ? convert(bytes, length, "UTF-8", encoding) : nullptr;
}

}
