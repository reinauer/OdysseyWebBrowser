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
emulator coverage. There is still no full browser build, JavaScript
execution, page rendering, physical-hardware test or browser memory
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

Build the standalone native integration test (this does **not** build a
browser executable):

```sh
cmake -S Tools/AmigaOS3 -B build/amigaos3-platform \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/Source/cmake/AmigaOS3.cmake" \
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
This does not exercise browser UI callbacks, rendering or GC suspension.

## Full browser configuration and remaining work

The browser can now be configured with the OS3 toolchain:

```sh
cmake -S . -B build/amigaos3-browser \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/Source/cmake/AmigaOS3.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DPORT=MUI \
  -DAMIGAOS3_DEPENDENCY_ROOT=/path/to/m68k-dependencies
```

This currently **stops at missing ICU >= 61.2** (headers and the data,
i18n and uc libraries) in the installed SDK. The OS3 path resolves real
target dependencies and does not synthesize AROS library paths or accept
host libraries. An empty framework-property CMake parsing error encountered
during bring-up was also corrected.

Next work:

1. Build/stage m68k ICU, then the remaining target dependencies (Cairo,
   Fontconfig, curl/TLS, SQLite, XML/XSLT, HarfBuzz, image codecs and PSL).
   Check SDK-supplied libraries against the selected CPU and C runtime.
2. Complete the WTF thread backend. The SDK pthread smoke passes, but the
   existing `ThreadingPOSIX.cpp` still needs its OS3 signal/suspension,
   register capture and GC coordination paths. Audit atomics and TLS too.
   Do not equate basic pthread operation with working WebKit threading.
3. Port filesystem and CPU-time services and remaining low-level APIs;
   compile and execute the C-loop JavaScriptCore on m68k.
4. Port browser library startup, MUI callbacks/hooks, DOS/graphics,
   clipboard, sockets and character conversion. Classify each OS4-specific
   API before sharing it with OS3. Establish an OS3 optional-feature policy
   separately from OS4 multimedia/GPU support.
5. Bring up local HTML, then network browsing; measure resource usage on
   emulated and physical systems before choosing resource limits.

The base is an older WebKit lineage despite the recent Odyssey release;
this branch does not claim current upstream WebKit security coverage.
