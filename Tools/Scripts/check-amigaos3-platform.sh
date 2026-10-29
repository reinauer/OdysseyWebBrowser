#!/bin/sh
# Compile the real platform headers with an AmigaOS 3 cross-compiler.
set -eu

compiler=${1:-m68k-amigaos-g++}
source_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
probe_dir=$(mktemp -d "${TMPDIR:-/tmp}/odyssey-amigaos3.XXXXXX")
trap 'rm -rf "$probe_dir"' EXIT HUP INT TERM

cat > "$probe_dir/platform.cpp" <<'EOF'
#include <wtf/Platform.h>

#if !CPU(M68K) || !CPU(KNOWN) || CPU(UNKNOWN)
#error "Expected a recognized m68k CPU"
#endif
#if !CPU(BIG_ENDIAN) || !CPU(ADDRESS32) || !CPU(REGISTER32)
#error "Expected big-endian 32-bit addressing and registers"
#endif
#if !CPU(NEEDS_ALIGNED_ACCESS)
#error "m68k must retain conservative aligned accesses"
#endif
#if !OS(AMIGAOS3) || OS(AMIGAOS) || OS(AROS) || OS(MORPHOS)
#error "Classic AmigaOS must not select another Amiga-family OS"
#endif
#if !PLATFORM(MUI) || !USE(SYSTEM_MALLOC)
#error "Expected MUI and the C library allocator"
#endif
#if !USE(JSVALUE32_64) || USE(JSVALUE64) || !ENABLE(C_LOOP)
#error "Expected the 32-bit C-loop interpreter"
#endif
#if ENABLE(JIT) || ENABLE(DFG_JIT) || ENABLE(FTL_JIT) || ENABLE(YARR_JIT) || ENABLE(WEBASSEMBLY)
#error "Unsupported code generators must remain disabled"
#endif
static_assert(sizeof(void*) == 4, "Expected 32-bit pointers");
static_assert(sizeof(double) == 8, "Expected 64-bit doubles");
EOF

"$compiler" -std=c++17 -I"$source_root/Source/WTF" \
    -c "$probe_dir/platform.cpp" -o "$probe_dir/platform.o"
# Older compilers only advertise the generic AmigaOS target macros.
"$compiler" -std=c++17 -U__amigaos3__ -I"$source_root/Source/WTF" \
    -c "$probe_dir/platform.cpp" -o "$probe_dir/platform-legacy.o"
printf '%s\n' 'AmigaOS3 platform compile checks passed (modern and legacy macros).'
