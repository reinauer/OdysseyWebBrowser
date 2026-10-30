/* Verify that native int uses the existing fixed-width persistence format. */
#include "config.h"
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <climits>
#include <cstdio>
#include <cstring>

int main()
{
    WTF::Persistence::Encoder native;
    WTF::Persistence::Encoder fixed;
    native << INT_MIN << UINT_MAX << INT_MAX << 0U;
    fixed << static_cast<int32_t>(INT_MIN) << static_cast<uint32_t>(UINT_MAX)
          << static_cast<int32_t>(INT_MAX) << static_cast<uint32_t>(0);
    native.encodeChecksum();
    fixed.encodeChecksum();
    if (native.bufferSize() != fixed.bufferSize()
        || std::memcmp(native.buffer(), fixed.buffer(), native.bufferSize()))
        return 1;

    WTF::Persistence::Decoder decoder({ native.buffer(), native.bufferSize() });
    std::optional<int> minimum, maximum;
    std::optional<unsigned> unsignedMaximum, zero;
    decoder >> minimum >> unsignedMaximum >> maximum >> zero;
    if (!minimum || *minimum != INT_MIN || !unsignedMaximum || *unsignedMaximum != UINT_MAX
        || !maximum || *maximum != INT_MAX || !zero || *zero || !decoder.verifyChecksum())
        return 2;

    WTF::Persistence::Decoder truncated({ native.buffer(), 3 });
    minimum = 42;
    truncated >> minimum;
    if (minimum)
        return 3;
    std::puts("PASS native persistence integer format");
    return 0;
}
