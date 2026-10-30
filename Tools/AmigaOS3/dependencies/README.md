# Native browser dependencies

Use [SDK-PROFILES.md](../SDK-PROFILES.md) for the matched October 29 runtime,
ICU and dependency build order. Do not combine the mutable
`/opt/amiga-16.2` symlink with the original October 14 dependency/runtime
prefix. After building the matching runtime and ICU, build the non-ICU
stack with explicit inputs:

```sh
python3 Tools/AmigaOS3/build-dependencies.py \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-root build/amigaos3-deps-oct29 \
  --prefix build/amigaos3-sdk-oct29 \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-oct29 --smoke
```

The script's historical defaults are compiler `/opt/amiga-16.2`, build root
`build/amigaos3-deps-other` and install prefix `build/amigaos3-sdk`, shared
with `build-icu.py`. Those old paths and their evidence remain intact;
the explicit command above selects the separate current profile instead.
This script does
not replace that prefix or modify the compiler installation. It requires
Python 3.12+, CMake, Ninja, make, pkg-config, curl and the native build tools
used by the upstream releases. Individual package names can be supplied to
resume a build; their prerequisites must already be staged.

To relink existing probes after updating the runtime without rebuilding or
modifying the staged dependency archives:

```sh
python3 Tools/AmigaOS3/build-dependencies.py --probes-only \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-root build/amigaos3-deps-oct29 \
  --prefix build/amigaos3-sdk-oct29 \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-oct29 \
  --smoke --font-smoke --sqlite-smoke --sqlite-volume DH1: --psl-smoke
```

Add `--file-smoke --file-volume DH1:` to include the focused filesystem
regression, or select `RAM:` for the RAM filesystem. These probes use the
same `run-platform-test.py` workflow; the file test's success marker is
`PASS native filesystem links and metadata`.

Every probe links the runtime's explicit compiler helper objects (the four
floating-point repairs and, when present, the fifth integer-math object),
plus native Malloc and TTY wrappers before libraries. Their hashes and the
resulting executable hashes are recorded in the dependency manifest. The
`DH1:` SQLite probe requires the native FFS fixture described in [SQLite.md](SQLite.md).

Pinned, checksum-verified sources build zlib 1.3.1, libpng 1.6.47, FreeType
2.12.1, pixman 0.40.0, Expat 2.7.1, fontconfig 2.14.2, Cairo 1.16.0,
libxml2 2.12.10, libxslt 1.1.43, HarfBuzz 8.3.0, WebP 1.5.0,
libjpeg-turbo 3.1.0, libpsl 0.21.5 and SQLite 3.46.1. Build/stage ICU
before HarfBuzz and libpsl, which use its Unicode and IDNA implementations.
HarfBuzz includes the real `libharfbuzz-icu.a` adapter. Build the native
pthread C++ runtime with `build-cxx-runtime.py` first: its headers and
archives are selected from `PREFIX/cxx-runtime` or `--cxx-runtime`.
These are development
port dependencies, not a claim
that each is the latest or suitable for a security release. Source archives
and upstream licenses are retained. `manifest.json` records compiler,
source and archive hashes; each command has its own log.

The SDK's supplied zlib/PNG/FreeType archives cannot be reused with its
default newlib: they reference libnix symbols, and FreeType also exports
bundled zlib symbols. All three are rebuilt from source. The script isolates
pkg-config from host libraries and verifies installed archives are nonempty.

## Target adaptations

* Pixman uses pthread keys rather than `__thread`: the supplied compiler's
  emulated TLS is not isolated between pthreads. Its legacy int/int32_t
  raster loops require disabling strict aliasing on the 32-bit Amiga ABI.
* Fontconfig uses the native directory adapter and its existing mkstemp
  fallback. SDK pthread macros must not expand FreeType callback member
  calls. The SDK has `random` but lacks its associated state functions;
  fontconfig selects its existing `lrand48` fallback.
* Cairo retains pthread locking. Its automatic fortification option is
  omitted because the SDK newlib fortified declarations do not compile.
* libxml2 omits its separate network, gzip-file and iconv adapters. Browser
  resource loading/decompression and ICU text conversion remain separate
  responsibilities. WebP uses its supported serial codec implementation.
* JPEG builds the library and headers, avoiding cross-execution of tools.

`libodyssey-amigaos3.a` supplies real AmigaDOS directory enumeration,
file metadata, timestamps, access checks, directories, paths, links and the
record-lock subset of `fcntl`. Unsupported `fcntl` requests fail with
`EINVAL`; this is not a complete POSIX compatibility library. AmigaDOS has
one modification timestamp and owner protection bits, which are mapped to
the available POSIX metadata. `stat` enumerates the parent directory when
an open `MODE_NEWFILE` file holds an exclusive lock.

`lstat` enumerates the parent without locking the final entry, distinguishes
`ST_SOFTLINK`, and obtains the link length through `ReadLink` using the parent
handler, lock and basename. Dangling links therefore retain their own
metadata. `stat` also follows a link when its target is exclusively open:
the parent-enumeration fallback resolves the link relative to that parent,
with a bounded traversal, before examining the target. Public `link` and newlib's `_link`
both use native hard links; FFS's `OBJECT_IN_USE` result for an existing
destination is translated to `EEXIST` only after confirming the entry exists.
AmigaDOS's public file information does not expose a hard-link count; the
adapter continues to report `st_nlink = 1`.

The October 29 SDK defines public `isatty` in the same object as `open`, but
only tests whether the descriptor number is at most two. That misidentifies
redirected standard streams. `Source/WTF/wtf/amigaos3/TTY.c` supplies
`__wrap_isatty`, using the checked DOS handle and `IsInteractive`, with
`EBADF` and `ENOTTY` errors. Link that object explicitly and pass
`-Wl,--wrap=isatty`; a weak replacement would silently select the faulty SDK
implementation. `curl-files.c` retains the real `ftruncate` adapter.

The newlib descriptor bridge calls `BPTR __check_fno(unsigned)` instead of
inspecting the descriptor table. This accessor is documented in
[AmigaPorts newlib open.c](https://github.com/AmigaPorts/newlib-cygwin/blob/amiga/newlib/libc/sys/amigaos/open.c).
The inspected source SHA256 is
`7990d3eb12cdf6b6e94213720f6b0e81979391d01b17a8a65ee040f6b367a10e`.
The script checks the accessor symbol and records the installed libc hash.
The SDK's `_fstat` merely returns success without populating metadata;
the adapter provides genuine `fstat`/`stat` and their reentrant stdio entry
points. Link the support archive before libc; the staged `.pc` files carry
its private static dependencies along with pthread and libatomic.

## Validation

`--smoke` links `dependency-smoke` in the selected build root through the
staged static pkg-config interfaces. Run it in the existing isolated guest
workflow, with success marker `PASS dependency smoke` and a stack of at
least 256 KiB. It checks Cairo/pixman ARGB pixels, PNG encoding, fontconfig
patterns, XML parsing and XSLT transformation, directory enumeration,
file sizes/types/timestamps, DOS record locking and errno failures.

The original profile's graphics/XML/filesystem smoke passed in the 68020 AmigaOS 3.2 WinUAE
guest: `build/amigaos3-dependencies-final/output.txt`, with input hashes in the
adjacent `inputs.json`. This does not establish complete font discovery,
browser rendering, networking or physical hardware correctness. WebP and
JPEG archives compile, but are outside that runtime smoke.

The focused `file-smoke.c` checks open-file stat/fstat/lstat, timestamps,
zero-filled extension and truncation, hard-link contents after deleting the
original name, symbolic and dangling-link metadata, creation errors, and
actual redirected-stdio and invalid-descriptor detection. Its final native
RAM run passed 45 checks in `build/amigaos3-files-ram-final/output.txt`; the
FFS run is recorded in `build/amigaos3-files-ffs-final/output.txt`. These focused C
probes use the October 29 compiler and the production filesystem, TTY and
time adapters, independently of the browser's C++ dependency rebuild.

The matched-SDK support library uses linker wrappers for `getcwd`, `utimes`
and `utime`: SDK `chdir.o` and `utime.o` bundle competing definitions.
Executables that link `AmigaOS3::Support` inherit
`-Wl,--wrap=getcwd,--wrap=utimes,--wrap=utime`; standalone platform executables
without that library do not. Manual support-library links need the same flags.
`getcwd` resolves the calling task's current-directory lock with `NameFromLock`,
not the CLI's potentially empty/stale cached name. `utime` delegates to the
native `utimes` UTC/local/DST conversion; AmigaDOS stores modification time
only. This adds no process-wide replacement for `chdir`.

The extended filesystem probe passes 59 checks on both RAM and real FFS:
`build/amigaos3-files-cwd-utime-fixed/output.txt` and
`build/amigaos3-files-cwd-utime-ffs/output.txt`. It adds SDK `chdir` plus
allocated/small-buffer `getcwd`, restoration of the original directory, and
explicit/null `utime` and `utimes` under `DST_OFFSET=3600`, retaining the
existing exclusively open-file and link tests. Each runner records exact
binary/input hashes; this is native WinUAE evidence, not physical hardware.

After the final runtime repairs, all four relinked probes passed again with
the four compiler helper objects and malloc locking enabled. Current native
outputs are `build/amigaos3-dependencies-four-helpers/output.txt`,
`build/amigaos3-fonts-four-helpers/output.txt`,
`build/amigaos3-sqlite-four-helpers/output.txt` (native FFS `DH1:`), and
`build/amigaos3-psl-four-helpers/output.txt`.

`--font-smoke` stages an OFL-licensed Noto Sans font from a pinned upstream
commit and links `font-smoke`. Copy `build/amigaos3-sdk/share/amigaos3/Conf`
and `Fonts` beside that executable in the guest. Run with success marker
`PASS native font smoke`. The text smoke passed in
`build/amigaos3-fonts-1/output.txt`: fontconfig discovered the installed
font, FreeType/HarfBuzz shaped `office` into four glyphs, and Cairo rendered
`Odyssey` into 559 covered pixels. `system/font-smoke.png` was inspected.
After enabling the ICU adapter and rebuilding with the pthread C++ runtime,
the extended test passed in `build/amigaos3-fonts-icu-1/output.txt`. It also
checks ICU-backed Unicode properties and uses those functions for shaping.

Runtime configuration resides at `PROGDIR:Conf/fontconfig/fonts.conf` and
scans `PROGDIR:Fonts` plus `FONTS:_TrueType` when present. Cache files reside
in `T:fontconfig`. The fontconfig port recognizes Amiga assigns, uses
semicolon-separated `FONTCONFIG_PATH` lists, avoids adding a slash after
an assign colon, and preserves double slash parent-directory semantics.
The staged config contains no host build paths.

`--psl-smoke` links the public-suffix test. Native public suffix, cookie
domain boundaries, IPv6 and Chinese IDN checks passed in
`build/amigaos3-psl-1/output.txt`. Missing `inet_pton` is supplied by the
actual musl 1.2.5 implementation; its MIT license is retained in
`MUSL-COPYRIGHT` and staged with the dependency licenses.

`--sqlite-smoke --sqlite-volume=DH1:` links the transaction test for a native
FFS volume. See [SQLite.md](SQLite.md) for the VFS contract, native evidence
and limitations. Curl/AmiSSL is built separately from this graphics and
database dependency workflow.
The staged `Conf` and optional `Fonts` directories need to be included in
the eventual Odyssey release packaging.
