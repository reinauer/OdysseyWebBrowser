# AmigaOS 3 port baseline

## Decision

Use **AmigaLabs Odyssey 3.2.2**, based on
`odyssey-webkitgtk-2.34.6+wayfarer-3`, as the base of `AmigaOS3`.
The exact base is `a202d810158265daed814fee7912db434a1ce353`, also tagged
`odyssey_3.2.2`. Browser and engine versions are different: `versioninfo.h`
identifies Odyssey 3.2.2 and WebKit 605.1.15, with the branch describing
its WebKitGTK 2.34.6 / Wayfarer 3 source lineage.

This preserves the newer engine and AmigaLabs' OS4 work while adapting the
platform layer to classic m68k AmigaOS. Its existing 32-bit big-endian PPC
paths and C-loop configuration are useful references, not proof of m68k
compatibility. The OS4 implementation depends on PPC, clib4, OS4 services,
and optional GPU/video interfaces which cannot simply be enabled on OS3.

The initial 2.24.4 experiment is retained as `AmigaOS3-webkit-2.24.4`.
Its older engine might reduce initial bring-up work, but there is no measured
RAM or speed advantage, and starting there adds a later engine migration.
The chosen priority is to port the platform layer onto 3.2.2 directly.

## Branch survey (2026-10-29)

Fetched all advertised heads from `deadwood2/OdysseyWebBrowser` (origin)
and `AmigaLabs/OdysseyWebBrowser` (local `amigalabs/*` refs). The latter
were fetched by URL; no persistent remote was added. Every head is listed
below. The assessment combines history, build configuration and targeted
platform code inspection; it is not a build/runtime comparison of every
engine version. Identical hashes mean the repositories share that head.

| Branch | Origin head | AmigaLabs head | Assessment |
| --- | --- | --- | --- |
| `webkit` | `5c36186839` | `357931cfc7` | Engine import history, without the complete Odyssey integration; not a browser base. |
| `master` | `66a4a1de40` | `7ee89cf4c1` | Integration/import branch, not the complete port. Origin is at WebKit 2.46.1/Wayfarer 9; AmigaLabs has an older integration head. |
| `odyssey` | `8e44b6d8d2` | `8e44b6d8d2` | Same head as 2.24.4 in both repositories; older fallback baseline. |
| `jaokim_os4_compile` | `d1522a204a` | `d1522a204a` | 2019 OS4 compile experiment; useful API reference, superseded by the current AmigaLabs OS4 work. |
| `kas1e_branch` | `05373fd3e5` | `05373fd3e5` | 2016 OS4 work on the old engine; reference only. Its proxy password ID fix is already present in 3.2.2. |
| `odyssey-r183644` | `d17cbb7852` | `d17cbb7852` | Old engine and MorphOS threading integration; possible native Exec reference, poor browser baseline. |
| `odyssey-r185045` | `04159ed9e1` | `04159ed9e1` | Old engine with theme changes; same age/platform limitations. |
| `odyssey-r187682` | `c2deb96285` | `c2deb96285` | Old engine with pthread integration and assertion workarounds; no native OS3 target. |
| `odyssey-webkitgtk-2.10.9` | `b474eb97e2` | `b474eb97e2` | Older engine, pthread integration; historical fallback, not preferred for current browsing. |
| `odyssey-webkitgtk-2.12.5` | `0e0d105df5` | `0e0d105df5` | Older engine; includes keyed encoder/decoder backport. Not preferred over the released 3.2.2 tree. |
| `odyssey-webkitgtk-2.14.7` | `68f0cd7f1c` | `68f0cd7f1c` | Intermediate engine migration; tip explicitly describes broken CryptoDigestGnuTLS for linking. |
| `odyssey-webkitgtk-2.16.6` | `cf8ea02426` | `cf8ea02426` | Intermediate migration with the same CryptoDigestGnuTLS caveat. |
| `odyssey-webkitgtk-2.18.6` | `06bdf11f6f` | `06bdf11f6f` | C++14-era migration with the same CryptoDigestGnuTLS caveat. |
| `odyssey-webkitgtk-2.20.5` | `c1605b779a` | `c1605b779a` | Intermediate interface-adjustment branch; no OS3 integration advantage found. |
| `odyssey-webkitgtk-2.22.7` | `0173b769f6` | `0173b769f6` | Intermediate branch with POISON disabled; no OS3 integration advantage found. |
| `odyssey-webkitgtk-2.24.4` | `8e44b6d8d2` | `8e44b6d8d2` | Older default baseline, C++14-era compiler configuration and non-x86-64 C-loop defaults. Initial bring-up alternative only. |
| `odyssey-webkitgtk-2.26.4` | `fae912056c` | `fae912056c` | Intermediate migration with cumulative changes at its tip; no reason to stop here instead of 3.2.2. |
| `odyssey-webkitgtk-2.28.4` | `2100bf3c59` | `2100bf3c59` | Migration tip explicitly marks functionality broken/disabled by rebasing. |
| `odyssey-webkitgtk-2.30.6` | `4ef22a851b` | `4ef22a851b` | Further migration and shutdown work; useful source history, not the selected base. |
| `odyssey-webkitgtk-2.32.4` | `65438bccf0` | `65438bccf0` | Further migration, C++17 compiler checks, and memmem compatibility implementation. Later branch already contains that implementation. |
| `odyssey-webkitgtk-2.34.6+wayfarer-3` | `01011649e8` | `a202d81015` | AmigaLabs head is Odyssey 3.2.2: selected base. Origin has independent fixes worth reviewing individually. |
| `odyssey-webkitgtk-2.48.3+wayfarer-9` | — | `f8a205163d` | AmigaLabs-only experiment whose tip says many things remain to fix. Not a released replacement for 3.2.2. |

## Imported fixes and portability changes

AmigaLabs changes are inherited through the exact 3.2.2 base, including
native context menus, configurable user commands, script preference parsing,
and its networking/thread fixes. Replaying those commits would duplicate
existing changes.

Two independent origin fixes are cherry-picked with original authors and
source hashes recorded in their commit messages:

- `01011649e86319aaa0279c30c2c8422792cba432`: guard disk-object tooltype
  reads and disposal when the program's icon is missing.
- `031876e8187b8deacbee75591e4f7d01332ce045`: restore supported MIME types
  in preferences and recognize the shtml/shtm HTML extensions.

The old OS4 proxy password identifier fix is already present. The cache
index last-line fix (`015a6e102e`) is already present in equivalent form.
The newer `memmem.c` compatibility implementation is also already present.
Large cumulative engine, allocator, network and media commits have not been
replayed merely because their hashes differ: rebased histories contain
related changes, and importing them requires checking behavior and APIs.

The initial OS3 configuration change recognizes `CPU(M68K)` and
`OS(AMIGAOS3)`, selects MUI/system malloc, and adds CMake CPU recognition.
It retains conservative aligned-access handling. `OS(AMIGAOS)` still means
the existing OS4 paths; pretending OS3 is OS4 would select incompatible
interfaces. The existing interpreter defaults select C-loop/JSVALUE32_64.
The first native backend is described below. The complete browser still
requires further platform adaptation and target dependencies.

## Validation

Run the reproducible target-header check from the repository root:

```sh
sh Tools/Scripts/check-amigaos3-platform.sh m68k-amigaos-g++
```

Initially passed with GCC 13.4.0b; also passed with the selected
`/opt/amiga-16.2/bin/m68k-amigaos-g++` (GCC 16.2.0b). The probe compiles the real platform headers to
m68k object files and checks CPU/OS identity, 32-bit pointers, big-endian
selection, conservative alignment, MUI, system allocation, JSVALUE32_64,
C-loop, and disabled JIT/DFG/FTL/Yarr JIT/WebAssembly. It also checks the
fallback for compilers without `__amigaos3__`.

Cherry-picks applied without conflicts. The MIME collection API and its
callers were inspected against this tree. `git diff --check` passes for
the branch changes. The native platform integration test below now adds
emulator coverage. Native JavaScript execution and collector stress now
pass as described below. There is still no full browser build, page
rendering, physical-hardware test or browser memory
benchmark. The target-header check alone does not validate these layers.

## First native platform milestone

The OS3 toolchain defaults to `/opt/amiga-16.2`, CPU 68020, and the MUI
headers already installed in that compiler SDK. Zune68 is used as the
runtime reference, without adding its AROS source include tree to the
browser build. Override `AMIGAOS3_TOOLCHAIN_ROOT` or `AMIGAOS3_CPU` through
CMake when needed.

Implemented and compiled from real WTF sources:

- Aligned allocation with overflow checks and matching free, because the
  SDK declares `posix_memalign` but does not provide a linkable definition.
  Preserve the JavaScriptCore `oomCrash` entry point without a weak alias,
  which this Hunk compiler cannot emit. Non-try allocations fail fatally
  instead of returning null to callers that require allocated memory.
- Physical-memory page reservations aligned to 4096 bytes, zeroing on
  reservation/recommit and release through the matching aligned allocator.
  OS3 has no virtual-memory reservation/decommit. Memory remains allocated
  until release; executable/JIT and protected guard-page requests fail
  explicitly instead of pretending to provide protection.
- Stack bounds from the current Exec task, including SDK pthread workers.
- `gettimeofday` and `timersub`, required by the SDK pthread archive;
  timer.device supplies local time, converted using the Amiga/Unix epoch
  difference, Locale GMT offset and Odyssey's existing `DST_OFFSET`
  convention. The OS3 WTF wall clock avoids applying that offset twice.
- Monotonic time from timer.device EClock, independent of wall-clock
  adjustments. Timer/library handles are local to each call for this
  initial implementation; persistent handles are a later optimization.

First build the pthread-enabled runtime with
`python3 Tools/AmigaOS3/build-cxx-runtime.py`. Then build the standalone
native integration tests (these do **not** build a browser executable):

```sh
cmake -S Tools/AmigaOS3 -B build/amigaos3-platform \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/Source/cmake/AmigaOS3.cmake" \
  -DAMIGAOS3_RUNTIME_ROOT="$PWD/build/amigaos3-sdk/cxx-runtime" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/amigaos3-platform -j4
```

`build/amigaos3-platform/odyssey-platform-test` is a native Amiga Hunk
executable. On the guest, use `Stack 262144` and run it with `--mui` to
include library opening and MUI text-object construction/disposal.

The checked-in runner copies an installed system into a new output
directory, installs the probe and optional Zune68 library, writes a minimal
startup sequence, starts an owned WinUAE process with GDB enabled, records
input hashes and output, and stops its process after completion. It does
not alter the source system, ROM or Zune68 checkout. Supply a working OS3
system directory with libraries matching the ROM. In particular, OS3.2
needs its matching icon/workbench libraries and a working datatypes.library.

```sh
python3 Tools/AmigaOS3/run-platform-test.py \
  --system /path/to/installed-amigaos3-system \
  --rom /path/to/matching-kickstart.rom \
  --emulator /path/to/winuae \
  --binary build/amigaos3-platform/odyssey-platform-test \
  --mui-library "$HOME/git/zune68/build/muimaster-mui5/muimaster.library" \
  --output build/amigaos3-run
```

The output directory must be new. The fixture uses an emulated 68020 with
2 MiB chip, 8 MiB fast and 64 MiB Zorro III memory; these are test resources,
not a browser minimum-RAM claim. Licensed ROM/system files stay local and
are not included in the repository.

The native test passed with `0 failures` and return code 0 on WinUAE with
an OS3.2 ROM and the local Zune68 library. It covers aligned/zero-size/
overflow allocations, page alignment and zeroing, partial recommit,
main/worker stack bounds and stack pointers, pthread create/join, timed
condition waits, monotonic and wall-clock calls, and MUI object creation.
The expanded tests also cover saved Exec register contexts, two-byte GC
root scanning, nested suspension, thread exit, and fixed-width arithmetic.
The browser's `vapor.h` macros also pass native custom-class
construction/disposal, method dispatch and `MUIM_CallHook` argument/return
checks with Zune68. Further checks exercise the SDK HookEntry trampoline,
superclass tag forwarding and MUI pointer-type selection. These component
checks do not validate the complete browser UI.

## Runtime and dependency bring-up

The original GCC SDK runtime is single-threaded even though it includes a
pthread archive. Native tests exposed shared `thread_local` storage and an
unsynchronized MemMap heap. The port builds a separate pthread-enabled
libstdc++ and emulated TLS implementation; it serializes the SDK allocator's
four reentrant entry points with an allocation-free recursive Exec semaphore.
The SDK installation is not modified. C++ runtime tests exercise TLS,
static initialization, once, mutexes, conditions, shared/weak ownership,
thread creation, sleeping and concurrent allocation. The SDK pthread
implementation also needed stable thread records and initialized TLS key
slots; creating another worker previously moved live cleanup-list sentinels.
See [runtime build and validation](Tools/AmigaOS3/RUNTIME.md).

OS3 thread snapshots use bounded `Forbid`/`Permit` regions while copying
saved Exec stacks. They include integer registers saved by Exec and scan
at two-byte strides, because m68k roots need not be four-byte aligned.
Allocation and buffer growth occur after tasks resume. Interrupts remain
enabled; other applications' task dispatch pauses during the copy. This
needs latency measurement with real JavaScript workloads.

ICU 62.2 is cross-built with its full big-endian data, matching the bundled
headers. Normalization, collation, word breaking, formatting and concurrent
locale-cache operations pass in the guest. See [ICU instructions](Tools/AmigaOS3/ICU.md).
The graphics/XML stack has native coverage for Cairo pixels/PNG output,
XML/XSLT, directory enumeration, file metadata/timestamps and record locks.
Fontconfig discovery, FreeType loading, HarfBuzz shaping and Cairo glyph
rendering also pass. See [dependency instructions](Tools/AmigaOS3/dependencies/README.md).
SQLite transactions and cross-process rollback-journal locking pass on
native RAM and FFS volumes. WinUAE's host-directory record-lock bug is
documented in [the SQLite notes](Tools/AmigaOS3/dependencies/SQLite.md).
AmiSSL 5.27 provides entropy through task-local library contexts; concurrent
native random-byte requests pass without weakening the random source.
The native curl/AmiSSL test passes verified HTTPS, rejects untrusted
certificates and wrong hostnames, and repeats worker initialization and
cleanup. See [network build and validation](Tools/AmigaOS3/CURL.md).
Native keyboard, dead-key, wheel and charset conversion tests also pass;
see [input support](Tools/AmigaOS3/KEYBOARD.md). Clipboard changes are
deferred from the device callback to a MUI handler on the browser task.

Build and stage dependencies with the selected runtime:

```sh
python3 Tools/AmigaOS3/build-cxx-runtime.py
python3 Tools/AmigaOS3/build-icu.py \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime
python3 Tools/AmigaOS3/build-dependencies.py --smoke
python3 Tools/AmigaOS3/build-curl.py --smoke
```

The engine-only build isolates JavaScriptCore from the MUI browser frontend:

```sh
cmake -S . -B build/amigaos3-jsc \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/Source/cmake/AmigaOS3.cmake" \
  -DAMIGAOS3_RUNTIME_ROOT="$PWD/build/amigaos3-sdk/cxx-runtime" \
  -DAMIGAOS3_DEPENDENCY_ROOT="$PWD/build/amigaos3-sdk" \
  -DCMAKE_BUILD_TYPE=Release -DPORT=MUI -DAMIGAOS3_JSC_ONLY=ON
cmake --build build/amigaos3-jsc -j4
```

The complete WTF and JavaScriptCore archives compile and the native `jsc`
shell runs. The script in `Tools/AmigaOS3/jsc-test.js` passed 2,142 checks
on the OS3.2 guest, including signed/unsigned boundaries, Float32 rounding,
IEEE overflow/NaN propagation, BigInt, typed-array byte order, Unicode
regular expressions, ICU formatting
and retained closures across 32 explicit garbage collections. Evidence is
in `build/amigaos3-jsc-ieee-final/output.txt` and its adjacent input
hash manifest. This is an engine integration test, not exhaustive GC proof.

JavaScriptCore's precise allocations now accommodate the SDK's four-byte
malloc alignment while preserving the engine's 16-byte header alignment
and tagged cell addresses. A separate native test passed 11,095 checks
covering allocation residues, growth, shrinkage, failure and lower-tier
reuse. The runtime overlay also replaces defective SDK numeric conversion
and double-arithmetic helpers with the actual GCC implementations. Native
math tests cover rounding, overflow, infinity, NaN and concurrent arithmetic.
See
[runtime details](Tools/AmigaOS3/RUNTIME.md).

Run the engine test with an installed AmiSSL 5.27 runtime in the fixture:

```sh
python3 Tools/AmigaOS3/run-platform-test.py \
  --system /path/to/installed-amigaos3-system \
  --rom /path/to/matching-kickstart.rom --emulator /path/to/winuae \
  --binary build/amigaos3-jsc/bin/jsc --script Tools/AmigaOS3/jsc-test.js \
  --stack 1048576 --z3-memory 256 \
  --success-marker 'Odyssey JSC test: 0 failures' \
  --output build/amigaos3-jsc-run
```

A working browser is not yet available. Use a fresh CMake
build directory when changing the toolchain/runtime flags. The OS3 build
resolves actual target dependencies and does not accept host libraries or
fabricated AROS SDK paths. For the full browser, omit `AMIGAOS3_JSC_ONLY`.

The complete WebCore static archive builds against the native dependency
archives, including the networking, font, image, layout and DOM sources.
Native UI helpers also have focused compilation and runtime coverage.
See [browser build notes](Tools/AmigaOS3/BROWSER.md).
The complete frontend now compiles and `OWB` links against the matched
October 29 SDK, runtime and dependencies. Native RTG blending and separator
drawing pass focused pixel checks. The native browser now opens and renders
the HTTP fixture, passing DOM/Unicode, CSS layout, canvas, JavaScript and
localStorage assertions. Further network, navigation and shutdown tests are
in progress. See [matched SDK profiles](Tools/AmigaOS3/SDK-PROFILES.md) and
[upstream runtime findings](Tools/AmigaOS3/UPSTREAM.md). Measure resource use
after startup and browsing work before choosing memory limits. The initial OS3
build disables video and MediaSource because no native FFmpeg stack is
staged. OS4 multimedia/GPU features need a separate port. Native component tests are not proof
of a complete browser, physical hardware behavior or a minimum-RAM target.

The base is an older WebKit lineage despite the recent Odyssey release;
this branch does not claim current upstream WebKit security coverage.
