#pragma once

#include <stddef.h>

namespace WebCore::AmigaOS3 {

// Returned strings use malloc/free. Invalid input or an unavailable converter
// returns nullptr; unrepresentable Unicode uses the local converter's substitute.
char* localToUTF8(const char*, size_t, size_t* outputLength = nullptr);
char* utf8ToLocal(const char*, size_t);
bool localEncodingName(char*, size_t);

}
