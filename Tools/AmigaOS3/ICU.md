# ICU for AmigaOS 3

Use the matched October 29 compiler, runtime and dependency prefixes in
[SDK-PROFILES.md](SDK-PROFILES.md). The mutable `/opt/amiga-16.2` symlink and
the scripts' original default output paths must not be combined with the
older October 14 runtime. Build the matched runtime first, then run from
the Odyssey checkout with Python 3.12 or newer:

```sh
python3 Tools/AmigaOS3/build-icu.py --toolchain /opt/amiga-16.2-20261029 \
    --build-root build/amigaos3-icu-oct29 \
    --prefix build/amigaos3-sdk-oct29 \
    --cxx-runtime build/amigaos3-sdk/cxx-runtime-oct29
```

The script downloads the official ICU 62.2 source archive, checks its SHA-256,
patches and builds it, and stages native 68020 static libraries and headers in
the selected prefix (`build/amigaos3-sdk-oct29` above). It preserves other
dependencies there. The original default `build/amigaos3-sdk` belongs to the
historical profile and is retained unchanged. Override
`--build-root`, `--prefix` and `--jobs` as needed; output directories must stay
inside the checkout. A changed source patch requires a fresh build root.

ICU 62.2 matches the headers already bundled with Odyssey 3.2.2 and exceeds the
build's ICU 61.2 minimum. It uses the SDK's pthread synchronization with the
project's runtime repairs. The source patch fixes assumptions that `int32_t`
is `int` (the SDK uses `long`) and uses the available C math functions. It does not disable
threading, converters, collation, normalization, break iteration or formatting.

Host tools and target objects are built in separate directories. The host
`icupkg` swaps the complete Unicode data package to big-endian ASCII form.
`genccode` emits portable C, which is compiled with the target compiler into
`libicudata.a`; no host object format or stub data enters the staged libraries.
The generic GCC configuration supplies static archive rules only. ICU's mmap,
popen, dynamic libraries and target tools are disabled.

The complete data archive is about 26 MiB. Trimming locales or converters is a
separate size decision; this initial port retains them. Upstream license text
is copied to `share/licenses/icu/LICENSE`. Source, patch, data, compiler and
artifact hashes are recorded in `icu-build.json` in the staged prefix.

The script also links `icu-test` in its target build directory. It checks data
initialization, NFC normalization, locale collation, word boundaries, decimal
formatting and two concurrent pthread workers using locale caches. Linking
requires `Source/WTF/wtf/amigaos3/Time.c`, the runtime overlay's `pthread`,
and the SDK's `atomic` library. It also compiles
`Source/WTF/wtf/amigaos3/Malloc.c` and wraps
`_malloc_r`, `_free_r`, `_calloc_r` and `_realloc_r`: the SDK's shared MemMap
heap bypasses newlib's usual malloc locks. Without this synchronization,
concurrent ICU operations corrupt the heap. Both shims are part of the OS3 WTF
integration. The runtime overlay also supplies explicit unsigned-to-float,
double-to-float and IEEE double arithmetic helper objects, overriding the
SDK libc math defects. The probe gives each worker a 256 KiB stack.

To build against the port's pthread-enabled C++ runtime, follow the matching
profile's `build-cxx-runtime.py` command, then pass that exact prefix as
`--cxx-runtime` to this script. It uses a
separate target build directory so objects built with the SDK's original C++
headers cannot be reused accidentally.

Run the executable in an AmigaOS 3 guest using `run-platform-test.py` with
`--success-marker 'ICU 62.2: 0 failures'` and the appropriate system, ROM and
emulator paths. A successful cross-build alone does not establish runtime
correctness. The runner records executable and emulator hashes with its output.

The original profile's native probe passed all six checks, including concurrent locale work,
in `build/amigaos3-icu-pthread-math-1/output.txt`.
The separately rebuilt October 29 result is
`build/amigaos3-icu-oct29/native/output.txt`; see the profile manifest for
its distinct executable and input hashes.


The OS3 browser startup gate in `mui/UI/icufiles.cpp` registers the linked data
entry point, initializes ICU and verifies both the library and data versions
against the build headers. It does not require `Resource/icudt49b` or a loose
ICU data directory. Initialization/version failures show a specific MUI error
and stop startup. Other platforms retain their existing file checks.

`ICUStartupTest.cpp` runs that actual gate twice with a real MUI library base.
The extracted browser compile/link commands and SDK identities are recorded in
`build/amigaos3-icu-startup/manifest.json`. Native outputs are in
`native-oct14/output.txt` (matched Oct14 SDK and overlay) and
`native-oct29/output.txt` (current Oct29 compiler/libc with the existing Oct14
runtime overlay and ICU). This separate mixed-SDK probe validates only the
startup component; it does not establish compatibility of the entire runtime.
