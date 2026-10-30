# SDK upstream audit

Read-only audit on 2026-10-29. Nothing was published. Our original native
failure evidence used `/opt/amiga-16.2-20261014`. During this session the
`/opt/amiga-16.2` symlink changed to `/opt/amiga-16.2-20261029` (AmigaDev
16.2-rc21). These are different SDKs; the original overlay must not be treated
as a list of unfixed bugs in the latest release.

Current primary-source revisions inspected:

- [AmigaPorts/newlib-cygwin, amiga, fb0a76ce](https://github.com/AmigaPorts/newlib-cygwin/tree/fb0a76ceacae0c7c1442d5a1e00be6ba7f669de2)
- [AmigaPorts/aros-stuff, master, bf81f53f](https://github.com/AmigaPorts/aros-stuff/tree/bf81f53fe4a3f309d8ebabaa893a1c740c39b3de)
- [AmigaPorts/gcc, amiga16.2, 1e6714c8](https://github.com/AmigaPorts/gcc/tree/1e6714c8f3756fc92ec93d4a3a25a49333b5407b)
- [AmigaPorts/m68k-amigaos-gcc, master, b1518f3b](https://github.com/AmigaPorts/m68k-amigaos-gcc/tree/b1518f3bea770b5e02150d3f603da7794eb0a6a6)

## Remaining upstream candidates

| Issue and owner | Current source finding | Small reproducer / existing evidence |
| --- | --- | --- |
| Allocator synchronization — AmigaPorts/newlib-cygwin | [`newlib/libc/sys/amigaos/mallocr.cpp`](https://github.com/AmigaPorts/newlib-cygwin/blob/fb0a76ceacae0c7c1442d5a1e00be6ba7f669de2/newlib/libc/sys/amigaos/mallocr.cpp) still has empty mutex functions. Its apparent RAII locks do nothing. The installed Oct29 allocator also has no real locking around MemMap mutations. | Two pthreads repeatedly allocate, fill/check, reallocate and free distinct buffers. `RuntimeTest.cpp` and the threaded `icu-test.cpp` exposed corruption with the old SDK; recursive Exec-semaphore wrappers fixed it. Keep the new-SDK baseline separate from those original native results. |
| TLS key allocation — AmigaPorts/aros-stuff | [`pthreads/pthread_key.c`](https://github.com/AmigaPorts/aros-stuff/blob/bf81f53fe4a3f309d8ebabaa893a1c740c39b3de/pthreads/pthread_key.c) still grows `_tlskeys` with `realloc` without initializing new slots. Delete/set-specific also calculate a slot pointer before acquiring the lock, allowing relocation between those operations. The Oct29 installed object retains the same growth pattern. | The isolated `PthreadKeyTest.c` reproduces both defects against the unchanged installed Oct29 archive: only 6/64 keys allocate with legal nonzero fresh storage; set/delete return EINVAL for valid keys when the table moves while they wait on its semaphore. See the controlled native comparison below. |
| Condition absolute timeout — AmigaPorts/aros-stuff | `pthread_cond.c` includes unsigned NDK `timeval` fields before the BSD `timersub` macro. The negative-microseconds branch is optimized away; the installed Oct29 object performs two subtractions without borrow normalization. Expired absolute deadlines also wrap into long unsigned timer waits. | Fresh-profile `PlatformTest.cpp` stopped at its condition timeout; `build/amigaos3-cond-time-oct29` separately hangs for a future deadline crossing a second. Disassembly is `build/amigaos3-probes/current-pthread-cond.dis`. The new-profile patch computes a signed wide seconds difference, normalizes the borrow, validates nanoseconds, and preserves mutex unlock/relock for expired waits. The repaired profile passes native cross-second, expired and invalid-nanosecond tests in `build/amigaos3-platform-oct29-fixed/output.txt`. |
| Double-to-float rounding — AmigaPorts/newlib-cygwin | [`__truncdfsf2.c`](https://github.com/AmigaPorts/newlib-cygwin/blob/fb0a76ceacae0c7c1442d5a1e00be6ba7f669de2/newlib/libc/sys/amigaos/__truncdfsf2.c) still delegates to `IEEEDPTieee`. Our native software-math fixture truncated where GCC's conversion requires nearest-even rounding. | Fresh unchanged Oct29 baseline reproduces truncation for volatile double `4294967295.0` and positive/negative odd halfway ties. See the current arithmetic comparison below; this is separate from the Oct14 `MathTest.cpp` evidence. |
| Compiler double arithmetic semantics — AmigaPorts/newlib-cygwin | [`__adddf3.c`](https://github.com/AmigaPorts/newlib-cygwin/blob/fb0a76ceacae0c7c1442d5a1e00be6ba7f669de2/newlib/libc/sys/amigaos/__adddf3.c), `__muldf3.c`, `__divdf3.c` and related overrides still delegate to native math libraries. Fresh Oct29 native tests with double-math libraries 38.1/37.1 saturate overflow at DBL_MAX rather than infinity. The tested NaN propagation and comparison cases pass in this configuration. | The unchanged SDK baseline below checks volatile `DBL_MAX * 2`, `DBL_MAX + DBL_MAX`, and `DBL_MAX / 0.5`; all saturate. It also distinguishes passing infinity/NaN operations from failing overflow. Old repaired-runtime evidence remains in `MathTest.cpp` and `jsc-test.js`; do not extrapolate it to other OS math-library versions. |

The original old-SDK failures are established native emulator results. Source
inspection shows the remaining implementations above are unchanged in the
relevant respects; it does not substitute for rerunning each minimal program
against the newly installed SDK before submitting a report.

Two additional Oct29 findings are independent of the older runtime evidence:

- [`open.c`](https://github.com/AmigaPorts/newlib-cygwin/blob/fb0a76ceacae0c7c1442d5a1e00be6ba7f669de2/newlib/libc/sys/amigaos/open.c)
  implements `isatty` by testing the descriptor number rather than the DOS
  handle. Redirected stdout is therefore reported as interactive, and invalid
  descriptors do not receive `EBADF`. The installed comparison is unsigned,
  so negative descriptors are rejected, also without `EBADF`.
  `TTY.c` uses the SDK descriptor accessor and
  `IsInteractive`, with `EBADF`/`ENOTTY`; the 45-check native filesystem probe
  covers redirected stdout and invalid descriptors on RAM and real FFS.
  Evidence: `build/amigaos3-files-ffs-final/output.txt` and
  `Tools/AmigaOS3/dependencies/file-smoke.c`.
- [`__divsi3.c`](https://github.com/AmigaPorts/newlib-cygwin/blob/fb0a76ceacae0c7c1442d5a1e00be6ba7f669de2/newlib/libc/sys/amigaos/__divsi3.c)
  exports `ldiv` alongside the compiler division helpers, while
  `newlib/libc/stdlib/ldiv.c` supplies another definition. A program using
  `ldiv` through a volatile function pointer and `__divsi3` fails to link
  against the installed Oct29 SDK with duplicate `ldiv` definitions.
  Reproducer and linker output: `build/amigaos3-division-audit/division.c`
  and `link.log`. Merely linking explicit GCC helpers is insufficient for
  the full browser: SDK `mktime.o` calls public `div`, which extracts the
  same alias bundle and then its `__udivsi4` dependency. The new profile
  also supplies the ordinary C `div` entry point in `integer-math.o`, keeping
  this bundle out while retaining newlib's separate standard `ldiv`.
  `integer-div-test.c` passed 45 native signed/unsigned boundary and ABI
  checks, including real `mktime` use, in
  `build/amigaos3-integer-alias/native/output.txt`. Its link map confirms
  neither conflicting compiler-helper archive member was extracted.

## Current unmodified SDK arithmetic baseline

The 2026-10-30 native run in
`build/amigaos3-sdk-math-baseline/native/output.txt` uses only the unchanged
`/opt/amiga-16.2-20261029` SDK (AmigaDev 16.2-rc21, GCC version string
`16.2.1b 20261010182711`). Its standalone C source is `Tools/AmigaOS3/SDKMathBaseline.c`, byte-identical
to the tested private `baseline.c` beside `manifest.json` and `link.map`. No Odyssey overlay, compiler repair objects,
allocator wrapper or pthread library is linked. The link map confirms that
conversion, arithmetic and comparison helpers come from the installed
`libm020/libc.a`. Volatile operands prevent constant folding; results are
printed as raw IEEE bit patterns, avoiding floating-point printf conversion.

The actual guest identity is Exec 47.7, AttnFlags `0003` (68020, no FPU),
`mathieeedoubbas.library` 38.1, `mathieeedoubtrans.library` 37.1 and
`mathieeesingbas.library` 47.1. The installed `version.library` reports 45.3.
This is the existing mixed installed-system fixture booted with the 3.2
A1200 ROM, not a claim that all of its disk libraries are from OS 3.2.
The manifest records SDK inputs, loaded versions and disk-library hashes;
the runner additionally hashes the ROM, emulator, configuration and every
copied guest file.

| Operation | Observed bits | Required bits |
| --- | --- | --- |
| Double 4294967295 to float | `4f7fffff` | `4f800000` |
| Double 1.000000178813934326171875 to float | `3f800001` | `3f800002` |
| Negative of that halfway value to float | `bf800001` | `bf800002` |
| DBL_MAX times 2, plus DBL_MAX, divided by 0.5 | `7fefffff:ffffffff` | `7ff00000:00000000` |

There are 18 checks and six mismatches. Infinity propagation through
addition, multiplication and division passes. Invalid infinity operations,
zero divided by zero, NaN propagation and all six NaN comparison predicates
also pass. Unsigned UINT_MAX converts to double correctly, confirming the
upstream unsigned-conversion repair. Gradual underflow and other rounding
modes were not tested by this minimal baseline. Zero exit status and the
`COMPLETE unmodified SDK arithmetic baseline` marker mean observation
completed; they do not mean the SDK passed the arithmetic checks.

```sh
/opt/amiga-16.2-20261029/bin/m68k-amigaos-gcc -m68020 -O2 -g -fno-builtin \
  Tools/AmigaOS3/SDKMathBaseline.c \
  -Wl,-Map,build/amigaos3-sdk-math-baseline/link.map -lamiga \
  -o build/amigaos3-sdk-math-baseline/baseline
python3 Tools/AmigaOS3/run-platform-test.py \
  --system build/amigaos3-amissl-fixture \
  --rom "$HOME/AmigaOS/AmigaOS-3.2-full/ROM/kicka1200.rom" \
  --emulator "$HOME/git/WinUAE_mac/WinUAE/bin/winuae" \
  --binary build/amigaos3-sdk-math-baseline/baseline \
  --output build/amigaos3-sdk-math-baseline/native-repeat \
  --gdb-port 2351 --success-marker 'COMPLETE unmodified SDK arithmetic baseline'
```

No new concurrent allocator failure is claimed by this run. The allocator
candidate above remains a source/disassembly finding for Oct29 plus separate
older native evidence; the single-thread baseline does not test its locking.

## Controlled native pthread TLS comparison

`PthreadKeyTest.c` isolates the key allocator in the installed October 29
SDK and the matching repaired pthread archive. `build-pthread-key-test.py`
links each archive without modifying it, producing separate allocation,
set-specific and deletion executables. The manifest pins the compiler,
libc, both pthread archives, source and executable hashes.

The allocator diagnostic uses `--wrap=realloc` only for the pinned
implementation's TLS table. In the allocation case it returns legal
nonzero contents in newly acquired storage; the library must initialize
its own `used` fields. In the two concurrency cases it always relocates
the table and quarantines cleared old storage. The main task holds the
actual TLS semaphore and waits until the worker is present in its Exec
wait queue before triggering growth. It therefore controls the precise
interleaving, rather than relying on repeated scheduling luck. The worker
makes ordinary valid `pthread_setspecific` or `pthread_key_delete` calls.
The set-specific test also reads back its stored value.

| Test | Unchanged Oct29 SDK | Repaired matching archive |
| --- | --- | --- |
| Allocate all keys | 6 of 64 keys, then `EAGAIN` | All 64 keys allocated |
| Set a valid key while table moves | `EINVAL` (22) | Success and correct value |
| Delete a valid key while table moves | `EINVAL` (22) | Success |

The native output directories are
`build/amigaos3-pthread-key-{baseline,repaired}-{allocation,setspecific,delete}`.
Each contains `output.txt` and complete runner input hashes. Baseline
executables explicitly expect and print `REPRODUCED native pthread TLS
defect`; repaired executables print `PASS native pthread TLS regression`.
Returning zero from a baseline executable means the defect was reproduced,
not that the SDK's pthread behavior passed.

```sh
python3 Tools/AmigaOS3/build-pthread-key-test.py
python3 Tools/AmigaOS3/run-platform-test.py \
  --system build/amigaos3-amissl-fixture \
  --rom "$HOME/AmigaOS/AmigaOS-3.2-full/ROM/kicka1200.rom" \
  --emulator "$HOME/git/WinUAE_mac/WinUAE/bin/winuae" \
  --binary build/amigaos3-pthread-key-audit/repaired-setspecific \
  --output build/amigaos3-pthread-key-repaired-setspecific \
  --gdb-port 2351 --success-marker 'PASS native pthread TLS regression'
```

Choose a new output directory when repeating a run. Substitute the other
binary names and the explicit baseline marker as appropriate. These are
native AmigaOS 3.2/68020 guest tests with a deliberately controlled allocator
and scheduler. They establish incorrect handling of valid memory-allocation
and synchronization outcomes, not their frequency with an ordinary heap,
physical-hardware behavior, or complete pthread correctness. The private
diagnostic declarations match `bf81f53f` and are not application APIs.

## Already fixed or materially changed upstream

- The unsigned conversion typo is fixed in current
  [`__floatunsidf.c`](https://github.com/AmigaPorts/newlib-cygwin/blob/fb0a76ceacae0c7c1442d5a1e00be6ba7f669de2/newlib/libc/sys/amigaos/__floatunsidf.c): it adds 2147483648 rather than 134217728.
  `__floatunsisf.c` now converts the unsigned range through double.
  The installed Oct29 archive contains the corrected double constant and
  float delegation. Do not report the missing-zero bug as still open.
- Current [`pthread_core.c`](https://github.com/AmigaPorts/aros-stuff/blob/bf81f53fe4a3f309d8ebabaa893a1c740c39b3de/pthreads/pthread_core.c)
  and `pthread_thread.c` allocate stable individual records instead of
  relocating live records and their list sentinels. `pthread_join` now releases
  its semaphore on invalid/detached-thread errors. The installed Oct29 archive
  contains the refactored implementation. Our old monolithic pthread patch
  must not simply be applied to this version.
- Oct29's installed C++ configuration enables `_GLIBCXX_USE_C99_MATH_FUNCS`.
  A fresh program using long-double `std::log2`, `log1p`, `round`, `trunc`,
  `hypot`, `nextafter` and `cbrt` compiled and linked without our adapter.
  Source and binary are in `build/amigaos3-upstream-audit/current-cmath.*`.
  This supersedes the old missing-declaration diagnosis; numerical runtime
  validation remains necessary before removing our old-SDK adapter.
- Current newlib [`s_nextafter.c`](https://github.com/AmigaPorts/newlib-cygwin/blob/fb0a76ceacae0c7c1442d5a1e00be6ba7f669de2/newlib/libm/common/s_nextafter.c)
  returns the destination when arguments compare equal, preserving the
  destination zero sign. Do not report the old equal-argument issue as
  present in this source without new evidence.

## Configuration and Odyssey portability

The current SDK still reports `--enable-threads=no`, `Thread model: single`,
and no `_GLIBCXX_HAS_GTHREADS`. The toolchain
[`Makefile`](https://github.com/AmigaPorts/m68k-amigaos-gcc/blob/b1518f3bea770b5e02150d3f603da7794eb0a6a6/Makefile)
already exposes a `THREADS` setting, defaulting to `no`. A tested pthread
runtime profile belongs in toolchain configuration/CI; GCC's single-thread
runtime is behaving according to that configuration. Compiler TLS, static
initialization and shared ownership need the pthread-enabled runtime for
Odyssey. This is separate from pthread's own TLS key bugs above.

`int32_t` being `long`, `uint32_t` being `unsigned long`, two-byte m68k stack
alignment, classic Exec register snapshots, and WebKit's stricter allocation
alignment and pointer-tag assumptions are port responsibilities. Submit general integer
API fixes to Odyssey/WebKit where useful; do not call those valid ABI choices
compiler defects. Likewise, explicit AmiSSL per-task initialization/socket
ownership and the native GC backend are Odyssey platform work, not evidence
of broken upstream ICU or generic OpenSSL.

The full-graph JSC run exposed another WebKit alignment assumption:
`LazyProperty` stored a callback in a native-aligned static object and used
two low pointer bits as tags. On m68k that object can be only two-byte
aligned, so decoding the tagged address read the wrong function pointer.
Explicitly aligning the storage to at least four bytes fixes the native
startup fault while preserving wider host alignment. This belongs in the
engine portability code, not in the compiler ABI. The repaired matched
shell passes 2,142 checks in `build/amigaos3-jsc-oct29-aligned/output.txt`.
The same minimum-alignment contract applies to `WordLock` stack queue nodes
and `WeakHandleOwner` objects. Their focused native regression reproduces
incorrect weak states and stalled contention before the fixes, then passes
all owner checks and 400 protected increments afterward. See the tagged
pointer section in [RUNTIME.md](RUNTIME.md).

## Recommended follow-up

The original Oct14 overlay remains intact. A separate fingerprinted Oct29
runtime and complete dependency rebuild now pass the component matrix in
[SDK-PROFILES.md](SDK-PROFILES.md). The newer profile patches only the affected
split pthread members and uses the matching SDK ABI throughout. Canonical GCC
arithmetic helpers remain selected consistently; that is not a claim that
the already corrected unsigned-conversion constant is still defective.

The standalone TLS baseline/repair tests, condition-timeout evidence,
division-link reproducer and current arithmetic baseline provide concrete
starting points for small upstream submissions. Include exact SDK and OS
identities, particularly the loaded math-library versions. For the allocator
report, retain the original native evidence but also reproduce the individual
failure against the current unmodified SDK; source inspection alone is
insufficient to label an old result as a new-SDK execution result.
No issue, PR or external message has been sent by this audit.
