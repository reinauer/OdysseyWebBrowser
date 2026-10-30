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
| Double-to-float rounding — AmigaPorts/newlib-cygwin | [`__truncdfsf2.c`](https://github.com/AmigaPorts/newlib-cygwin/blob/fb0a76ceacae0c7c1442d5a1e00be6ba7f669de2/newlib/libc/sys/amigaos/__truncdfsf2.c) still delegates to `IEEEDPTieee`. Our native software-math fixture truncated where GCC's conversion requires nearest-even rounding. | Convert a volatile double `4294967295.0` to float: expected `4294967296`, observed `4294967040`. Include halfway ties and signed values from `MathTest.cpp` / `RuntimeTest.cpp`. |
| Compiler double arithmetic semantics — AmigaPorts/newlib-cygwin | [`__adddf3.c`](https://github.com/AmigaPorts/newlib-cygwin/blob/fb0a76ceacae0c7c1442d5a1e00be6ba7f669de2/newlib/libc/sys/amigaos/__adddf3.c), `__muldf3.c`, `__divdf3.c` and related overrides still delegate to native math libraries. In the tested 68020/no-FPU AmigaOS3 fixture, overflow saturated at DBL_MAX rather than producing infinity; NaN/comparison behavior also needs the compiler helper contract. | Volatile `DBL_MAX * 2`, `DBL_MAX + DBL_MAX`, `0.0 / 0.0`, unordered comparisons and gradual underflow. `MathTest.cpp`, `build/amigaos3-c99-math-soft-2/output.txt` and `jsc-test.js` contain native evidence. GCC's own m68k software helpers fix these tests. Report exact OS math-library versions; this is not evidence that generic GCC arithmetic is broken. |

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

## Recommended follow-up

Retain the validated Oct14 compatibility overlay. Pin compiler, libc,
libpthread, libstdc++ headers and libraries as a matched SDK identity; do not
silently mix an old rebuilt runtime with a changed `/opt` symlink. In a fresh
Oct29 build directory, run the existing runtime, math, ICU, GC and JSC probes
against the unmodified SDK, then add only the still-required repairs. Keep the
old profile until that matrix passes. The new pthread source needs its own
small TLS-key patch, not replacement with the older full pthread object.

Only then remove superseded unsigned-conversion, stable-record/join and cmath
workarounds from the new profile. Upstream submissions should be small patches
with standalone native reproducers and exact SDK/OS identities. No issue, PR
or external message has been sent by this audit.
