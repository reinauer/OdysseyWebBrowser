# Matched SDK dependency profiles

## FPU profile

Recorded 2026-11-02. The browser is built for a 68020 to 68060 with an FPU:
`-m68020-60 -m68881`. Floating point results are returned in `fp0`, so the
runtime, ICU, every dependency and the browser must use the same profile;
`Tools/AmigaOS3/amigaos3_cpu.py` holds the flags and each builder takes
`--cpu fpu` (the default) or `--cpu soft` for the October 29 profile below.
The SDK selects its `libm020/libm881` libraries for these flags. The software
floating point helper objects are not built or linked.

| Input or output | Path |
| --- | --- |
| Compiler and SDK | `/opt/amiga-16.2-20261029` |
| Pthread C++ runtime | `build/amigaos3-sdk/cxx-runtime-fpu` |
| All browser dependencies | `build/amigaos3-sdk-fpu` |
| ICU objects and host tools | `build/amigaos3-icu-fpu` |
| Graphics, fonts, XML, PSL, nghttp2 and SQLite objects | `build/amigaos3-deps-fpu` |
| Curl objects | `build/amigaos3-curl-fpu` |
| Browser, `-O2` | `build/amigaos3-browser-fpu` |
| Browser, `-Os` (for comparison) | `build/amigaos3-browser-fpu-os` |

The SDK content fingerprint for these flags is
`4232fd40e97a74491fc39ceec48f65a6f296e3ac51734a2c9cab655922eaab99`.

ICU keeps 21 languages and their regional variants, the 19 converters WebKit
opens, and drops the Chinese/Japanese word dictionary, transliteration and
character names: 9,341,152 bytes of data instead of 25.6 MiB. The data is
writable, so it is linked into the data hunk rather than the code hunk.
`--full-data` restores the complete data.

```sh
python3 Tools/AmigaOS3/build-cxx-runtime.py --profile oct29 --cpu fpu \
  --toolchain /opt/amiga-16.2-20261029 \
  --gcc-source build/amigaos3-source-repos/gcc \
  --pthread-source build/amigaos3-source-repos/aros-stuff \
  --build-dir build/amigaos3-gthreads-fpu \
  --prefix build/amigaos3-sdk/cxx-runtime-fpu

python3 Tools/AmigaOS3/build-icu.py --cpu fpu \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-root build/amigaos3-icu-fpu \
  --prefix build/amigaos3-sdk-fpu \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-fpu

python3 Tools/AmigaOS3/build-dependencies.py --cpu fpu \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-root build/amigaos3-deps-fpu \
  --prefix build/amigaos3-sdk-fpu \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-fpu

python3 Tools/AmigaOS3/build-curl.py --cpu fpu \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-root build/amigaos3-curl-fpu \
  --prefix build/amigaos3-sdk-fpu \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-fpu

cmake -S . -B build/amigaos3-browser-fpu -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/Source/cmake/AmigaOS3.cmake" \
  -DAMIGAOS3_TOOLCHAIN_ROOT=/opt/amiga-16.2-20261029 \
  -DAMIGAOS3_DEPENDENCY_ROOT="$PWD/build/amigaos3-sdk-fpu" \
  -DAMIGAOS3_RUNTIME_ROOT="$PWD/build/amigaos3-sdk/cxx-runtime-fpu" \
  -DAMIGAOS3_PTHREAD_SOURCES="$PWD/build/amigaos3-gthreads-fpu/pthread-source/pthreads" \
  -DAMIGAOS3_NEWLIB_LICENSE="$PWD/build/amigaos3-browser-support/COPYING.NEWLIB" \
  -DCMAKE_BUILD_TYPE=Release -DPORT=MUI
cmake --build build/amigaos3-browser-fpu --target OWB jsc
```

`Release` now means `-O2`; `MinSizeRel` gives `-Os`. The toolchain sets
`AMIGAOS3_CPU_FLAGS`; the October 29 software floating point profile used
`-m68020`.

CMake turns the `-lm` in the curl, libxml2 and libxslt `.pc` files into an
absolute path. Without help it found `m68k-amigaos/lib/libm.a`, the 68000
soft-float library, which returns doubles in `d0/d1`; FPU code reads `fp0`.
The first FPU OWB therefore got garbage from `pow()` and aborted in
`MarkedSpace::sizeClasses()` while creating its JavaScript VM, and `jsc`,
which links `-lm` through the compiler driver, did not show it.
`AmigaOS3.cmake` now asks GCC for the multilib directory of
`AMIGAOS3_CPU_FLAGS` and puts it on `CMAKE_LIBRARY_PATH` after the runtime and
dependency prefixes, so `libm` resolves to `lib/libm020/libm881/libm.a` while
zlib, libpng and FreeType still come from the dependency prefix. An existing
build directory needs `cmake -U 'pkgcfg_lib_*' <build>` to forget the old
paths. `pow` and `log` in OWB and `jsc` must then have the same size.

`Source/WTF/wtf/amigaos3/Time.c` is also compiled into
`libodyssey-amigaos3.a`, and OWB links that copy. After changing `Time.c`,
rerun `build-dependencies.py`, or rebuild that one object with the
`native-time` command from its log and replace it with `m68k-amigaos-ar rcs`,
as was done on 2026-11-02.

The dependency command above did not use `--font-smoke`, which stages the
Noto Sans test font that `run-browser-test.py` requires. The font does not
depend on the CPU profile; it was copied from `build/amigaos3-sdk-oct29`
after its SHA-256 matched the values in `build-dependencies.py`.

## October 29 software floating point profile

Keep the original October 14 dependency prefix and runtime intact. The SDK
symlink changed during development, and October 29's newlib changed the
`_reent` layout from 240 to 76 bytes. Several old dependency archives also
referenced the removed `__locale_ctype_ptr` function. Replacing only those
archives would leave an incompletely audited mixture of SDK ABIs.

The complete October 29 rebuild uses these separate paths:

| Input or output | Path |
| --- | --- |
| Compiler and SDK | `/opt/amiga-16.2-20261029` |
| Pthread C++ runtime | `build/amigaos3-sdk/cxx-runtime-oct29` |
| All browser dependencies | `build/amigaos3-sdk-oct29` |
| ICU objects and host tools | `build/amigaos3-icu-oct29` |
| Graphics, fonts, XML, PSL and SQLite objects | `build/amigaos3-deps-oct29` |
| Curl objects and network probe | `build/amigaos3-curl-oct29` |

The SDK content fingerprint is
`047486a65ed6ae2711feedbd12187671e03edafe88d3e1c2963396b4f2f38823`.
This prefix predates HTTP/2: before configuring a browser with it, build
`build-dependencies.py --cpu soft ... nghttp2` and rerun `build-curl.py`, or
CMake stops at the missing `libnghttp2.a`.
The runtime manifest records the complete compiler, libraries, headers and
source revisions. Its split pthread archive replaces only `pthread_key.o`
and `pthread_cond.o`; the remaining SDK members are retained and checked.
See [RUNTIME.md](RUNTIME.md) and [UPSTREAM.md](UPSTREAM.md) for the repairs.

Reproduce the profile from the checkout root:

```sh
python3 Tools/AmigaOS3/build-cxx-runtime.py --profile oct29 \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-dir build/amigaos3-gthreads-oct29 \
  --prefix build/amigaos3-sdk/cxx-runtime-oct29

python3 Tools/AmigaOS3/build-icu.py \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-root build/amigaos3-icu-oct29 \
  --prefix build/amigaos3-sdk-oct29 \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-oct29

python3 Tools/AmigaOS3/build-dependencies.py \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-root build/amigaos3-deps-oct29 \
  --prefix build/amigaos3-sdk-oct29 \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-oct29 \
  --smoke --font-smoke --psl-smoke \
  --sqlite-smoke --sqlite-volume DH1: --file-smoke --file-volume DH1:

python3 Tools/AmigaOS3/build-curl.py \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-root build/amigaos3-curl-oct29 \
  --prefix build/amigaos3-sdk-oct29 \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-oct29 --smoke
```

Every manual probe links the five explicit compiler helper objects,
`Malloc.c`, and `TTY.c`, with the four allocation wrappers and `--wrap=isatty`.
Executables using native filesystem support additionally wrap `getcwd`,
`utimes` and `utime` to avoid bundled SDK symbol collisions.
The dependency builder's `--probes-only` mode relinks without modifying
dependency archives. Build logs and manifests retain commands and hashes.

Native component results on the final matched profile:

| Probe | Result | Evidence below `build/` |
| --- | --- | --- |
| ICU 62.2 | Six checks, including concurrent locale caches and formatters | `amigaos3-icu-oct29/native/output.txt` |
| Cairo/pixman, PNG, fontconfig, XML/XSLT, directory and metadata support | Passed | `amigaos3-deps-oct29/native-graphics-support/output.txt` |
| FreeType/HarfBuzz/ICU/fontconfig/Cairo | Font discovered; `office` shaped into four glyphs; 559 rendered pixels | `amigaos3-deps-oct29/native-fonts-support/output.txt` |
| Public suffixes | Cookie boundaries, IPv6 addresses and IDN passed | `amigaos3-deps-oct29/native-psl-support/output.txt` |
| Filesystem on real FFS | 59 checks, zero failures, including cwd and DST-independent timestamps | `amigaos3-deps-oct29/native-files-ffs-support/output.txt` |
| SQLite on real FFS | Commit/reopen, cross-process exclusion, rollback and creation guards passed | `amigaos3-deps-oct29/native-sqlite-ffs-support/output.txt` |
| Curl/AmiSSL before socket-error isolation | 21 checks; HTTP, verified HTTPS, certificate rejection and two worker lifecycles | `amigaos3-curl-oct29/native-support-final/guest/output.txt` |
| Curl/AmiSSL with native task-owned socket errors | Same 21 checks passed after rebuilding curl and its owner/worker helper | `amigaos3-curl-oct29/native-socket-error-final/guest/output.txt` |

Every listed guest returned zero; each result belongs to the executable
hash recorded by its runner. The current curl probe hash matches both its
build manifest and the copy actually executed.
`build/amigaos3-sdk-oct29/profile-validation.json` records
the SDK identity and hashes of the evidence; the package manifests record
archive and probe hashes. All public headers shared with the old dependency
prefix are byte-identical. None of the rebuilt archives references the
removed locale function; the full archive audit is saved beside the build.

The full-browser link subsequently exposed another SDK alias dependency:
`mktime` references `div`, which shares an archive member with already
replaced compiler helpers and `ldiv`. The current `integer-math.o` additionally
provides the ordinary C `div` entry point. Its focused native regression
passed 45 signed/unsigned boundary, ABI and `mktime` checks in
`build/amigaos3-integer-alias/native/output.txt`. The preceding runtime
manifest and object remain in that directory; earlier component and JSC
results remain tied to their original executable hashes. The aggregate
manifest records this follow-up separately rather than claiming those
earlier executables were rerun with the changed object.

The later socket-error correction rebuilds curl to read native bsdsocket
`Errno()` and set that channel through `SocketBaseTagList`, while the
production AmiSSL helper binds separate error storage to its owner and
worker tasks. The new curl archive is
`df533d56d6de1f0e845fbe79c920753932df736bd34770bee2f5b33fd27abf80`;
the executed probe is
`265c24c634cc7db65994a766c032dfc547d37505147e2cefb4edfb2f8322329d`.
The baseline archive and manifest remain under
`build/amigaos3-curl-oct29/socket-error-baseline`. The additive
`socket_errno_update` record in `profile-validation.json` preserves every
earlier output/hash and points to immutable before/after manifest copies in
`build/amigaos3-curl-errno-evidence`. The new 21-check result is a component
regression; the controlled errno-interleaving proof and actual browser
WebSocket/cancellation behavior remain distinct evidence. See
[UPSTREAM.md](UPSTREAM.md) and [CURL.md](CURL.md).

The focused division probe deliberately links the SDK's real `mktime` and
`ldiv` with the explicit integer helper object:

```sh
mkdir -p build/amigaos3-integer-alias
/opt/amiga-16.2-20261029/bin/m68k-amigaos-gcc -m68020 -O2 \
  Tools/AmigaOS3/integer-div-test.c \
  build/amigaos3-sdk/cxx-runtime-oct29/lib/integer-math.o \
  -Wl,-Map,build/amigaos3-integer-alias/link.map \
  -o build/amigaos3-integer-alias/integer-div-test
```

Run it with `run-platform-test.py` and success marker
`Integer division: 45 checks, 0 failures`.

Use [ICU.md](ICU.md), [CURL.md](CURL.md), and
[dependencies/README.md](dependencies/README.md) for the native runner
commands and fixture requirements. The font fixture needs staged `Conf`
and `Fonts` directories beside the executable. The FFS probes require a
private copy of the test hardfile mounted as `DH1:`. The curl fixture records
and corrects only its disposable guest clock; production certificate and
hostname verification remain enabled.

These are native component results, not proof of a functioning browser UI
or complete browsing session. Full-browser validation must select this
runtime and dependency prefix together and is recorded separately.
