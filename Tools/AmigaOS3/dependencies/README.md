# Native browser dependencies

Build the non-ICU dependency stack with the same 68020/newlib ABI as Odyssey:

```sh
python3 Tools/AmigaOS3/build-dependencies.py --smoke
```

The default compiler is `/opt/amiga-16.2`. Downloads, extracted upstream
sources, logs and objects are in `build/amigaos3-deps-other`. The install
prefix is `build/amigaos3-sdk`, shared with `build-icu.py`. This script does
not replace that prefix or modify the compiler installation. It requires
Python 3.12+, CMake, Ninja, make, pkg-config, curl and the native build tools
used by the upstream releases. Individual package names can be supplied to
resume a build; their prerequisites must already be staged.

To relink existing probes after updating the runtime without rebuilding or
modifying the staged dependency archives:

```sh
python3 Tools/AmigaOS3/build-dependencies.py --probes-only \
  --smoke --font-smoke --sqlite-smoke --sqlite-volume DH1: --psl-smoke
```

Every probe links the runtime's four explicit compiler helper objects and
the native malloc locking wrapper before libraries. Their hashes and the
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

`--smoke` links `build/amigaos3-deps-other/dependency-smoke` through the
staged static pkg-config interfaces. Run it in the existing isolated guest
workflow, with success marker `PASS dependency smoke` and a stack of at
least 256 KiB. It checks Cairo/pixman ARGB pixels, PNG encoding, fontconfig
patterns, XML parsing and XSLT transformation, directory enumeration,
file sizes/types/timestamps, DOS record locking and errno failures.

The graphics/XML/filesystem smoke passed in the 68020 AmigaOS 3.2 WinUAE
guest: `build/amigaos3-dependencies-final/output.txt`, with input hashes in the
adjacent `inputs.json`. This does not establish complete font discovery,
browser rendering, networking or physical hardware correctness. WebP and
JPEG archives compile, but are outside that runtime smoke.

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
