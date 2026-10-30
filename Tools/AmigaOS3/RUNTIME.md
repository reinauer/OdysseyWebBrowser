# AmigaOS3 C++ runtime and thread snapshots

The `/opt/amiga-16.2` compiler can generate the port, but its installed GCC
runtime was configured with `--enable-threads=no`. Its emulated compiler TLS
shares values across pthreads; its libstdc++ also selects unsynchronized
shared ownership and static initialization. The SDK provides real pthreads,
so the port builds a matching C++ runtime overlay in the workspace.

## Build the overlay

The recipe reads GCC sources from a local Git repository, without changing
its checkout or writing to `/opt`. The default checkout is
`~/git/AmigaPorts/gcc`. Obtain another checkout when needed:

```sh
git clone --branch amiga16.2 https://github.com/AmigaPorts/gcc.git gcc-amiga
git clone https://github.com/AmigaPorts/aros-stuff.git aros-stuff
python3 Tools/AmigaOS3/build-cxx-runtime.py --gcc-source gcc-amiga \
    --pthread-source aros-stuff
```

The default pinned source revision is
`e141137a28e4c36e45c8dc9064f413ddb17099bd`. The recipe builds the 68020,
static, pthread-backed libstdc++ with atomic shared-pointer ownership and
rebuilds GCC's actual `emutls.c` against its POSIX gthread adapter. Other
libgcc members come from the installed SDK; application C++ exceptions
remain disabled. This is the C++17 runtime for the port, not a replacement
for every compiler multilib or an independently validated exception runtime.

Defaults:

- Build files and logs: `build/amigaos3-gthreads`.
- Installed overlay: `build/amigaos3-sdk/cxx-runtime`.
- C++ headers: `include/c++` and `include/c++/m68k-amigaos`.
- SDK forwarding headers: `include`.
- Libraries: `lib/libstdc++.a`, `lib/libsupc++.a`, `lib/libnew_noexc.a`,
  `lib/libgcc.a` with its `emutls.o` member replaced, and `lib/libpthread.a`.
- Explicit emutls object, if required by an external build: `lib/emutls.o`.
- Compiler conversion overrides: `lib/floatunsidf.o`, `lib/floatunsisf.o`
  and `lib/truncdfsf2.o`.
- Build/source identity and SHA-256 hashes: `manifest.json`.
- GCC runtime licenses: `share/gcc-runtime`.
- Patched pthread source and license: `share/amiga-pthread`.

Use a fresh CMake build directory with
`-DAMIGAOS3_RUNTIME_ROOT="$PWD/build/amigaos3-sdk/cxx-runtime"`.
The toolchain selects the overlay headers with `-nostdinc++` and its
libraries with `-L`. External dependency builds must use the same headers
and runtime; shared ownership template types differ from the SDK's
single-thread configuration.

The forwarding `time.h` exposes the implemented nanosleep declaration
without advertising the entire POSIX timers API. The forwarding
`pthread.h` preserves its cancellation check while allowing qualified C++
calls. The SDK's fenv field named `__unused` is renamed in a copied header
to avoid an attribute-macro collision, preserving its layout.

`Source/WTF/wtf/amigaos3/Time.c` supplies real timer.device waits, UTC wall
time, the newlib `_gettimeofday` entry point, and EClock monotonic time.
The runtime selects those clocks explicitly. The recipe cleans previous
objects when regenerating configuration because libstdc++ does not track
all generated-header dependencies.

The SDK libc also overrides GCC's unsigned-to-double and unsigned-to-float
helpers with wrappers that add `0x8000000` where `0x80000000` is required.
Native conversion of `0x80000000` consequently produced `134217728`.
Its double-to-float wrapper also truncates through `IEEEDPTieee`, producing
the lower float instead of rounding to nearest even. The overlay builds
the three correct conversion helpers from the pinned GCC
`libgcc/config/m68k/fpgnulib.c`. The central CMake runtime module links these
objects before libc. External native executable links must likewise include
`lib/floatunsidf.o`, `lib/floatunsisf.o`, `lib/truncdfsf2.o` and
`lib/double-math.o` before libraries; `-L` alone does
not override libc's definitions. Runtime and math probes use volatile inputs
to test both conversions across the signed boundary and at `UINT_MAX`.

The fourth object contains GCC's software double arithmetic and matching
comparison predicates from the same pinned `libgcc/config/m68k/lb1sf68.S`.
The SDK libc delegates double arithmetic to mathieeedoubbas.library, which
returned DBL_MAX for both volatile `DBL_MAX * 2` and `DBL_MAX + DBL_MAX` in
the native fixture. This also broke `hypot` overflow. The maintained GCC
implementation restores IEEE infinity/NaN propagation, gradual underflow
and unordered comparisons. All six comparison entry points are supplied
together because the SDK comparison object aliases those symbols to one
implementation. No SDK installation files are changed.

GCC's private `_fpCCR` bookkeeping is process-wide. The supported soft-float
mode stays at default round-to-nearest with traps disabled; the SDK soft-float
`fenv.h` exposes no exception flags or alternate rounding modes. This work
does not claim thread-local floating-point exception state. Concurrent
arithmetic results are tested independently on four native pthreads.

## Bounded C99 math interface

The runtime installs `bits/amigaos3_c99_math.h` through its own `cmath` header.
It supplies the verified native float/double overloads of `round`, `lround`,
`hypot`, `nextafter`, `fmax`, `copysign`, `cbrt` and `trunc`, with integral and
mixed-argument promotion. The full `_GLIBCXX_USE_C99_MATH_FUNCS` switch remains
off: m68k long double is a distinct 12-byte format and most corresponding
extended-precision functions do not exist in this SDK. The newly added
long-double overloads are explicitly deleted, preventing silent narrowing.
GCC's existing C++17 three-argument hypot remains available; float/double
behavior is exercised by the native probe.

The nextafter adapter also corrects old newlib's equal-argument case by
returning the destination, including its sign for opposite signed zeros.
Other calculations call actual libm functions; double arithmetic is repaired
at the compiler-helper level rather than hidden in individual math calls.

Build the reproducible probe in the configured browser tree:

```sh
cmake --build build/amigaos3-browser --target amigaos3-math-test -j4
python3 Tools/AmigaOS3/run-platform-test.py \
  --system build/amigaos3-amissl-fixture \
  --rom /Users/stepan/AmigaOS/AmigaOS-3.2-full/ROM/kicka1200.rom \
  --emulator /Users/stepan/git/WinUAE_mac/WinUAE/bin/winuae \
  --binary build/amigaos3-browser/bin/amigaos3-math-test \
  --output build/amigaos3-c99-math-check \
  --success-marker 'math test: 0 failures' --gdb-port 2350
```

Use a new output directory and an unused port. Native evidence is in
`build/amigaos3-c99-math-soft-2/output.txt`: signed zero, ties, adjacent values,
subnormals, real versus intermediate hypot overflow, Inf/NaN propagation,
comparison predicates, cube roots and concurrent arithmetic all pass.
Compile-time assertions cover overload result types; separate negative
compiles verify rejection of direct and mixed long-double calls. The JSC
regression script also checks runtime overflow, NaNs and gradual underflow.
These are native emulator results, not physical-hardware validation.

## Stable pthread records

The SDK's pthread implementation reallocates its array of thread records
when more workers are created. Each record contains a cleanup list with
pointers to its own sentinel, and a running worker retains a record pointer
across its callback. Moving this storage invalidates both. A nested once
initializer running while the parent creates more workers reproduced the
resulting corruption. Installed SDK disassembly confirms the relocation
without list repair. Its dynamically grown TLS key array also leaves new
key slots uninitialized.

The overlay rebuilds `pthread.o` from AmigaPorts/aros-stuff revision
`86711c70569ffc6b70ee1fb99895866c1d9f30e4`, whose public header matches this
SDK byte for byte. `patches/pthread-stable-records.patch` uses an 8 KiB
fixed pointer table with individually allocated, reusable thread records,
and a small zero-initialized TLS key table. Records retain their addresses
until process exit. Error returns from join also release its semaphore.
The other SDK pthread archive members are retained. The default source
checkout is `~/old/git/m68k-amigaos-gcc/projects/aros-stuff`; override it
with `--pthread-source` as above.

## The SDK allocator also needs synchronization

The installed SDK's custom `malloc.o` uses shared MemMap allocation trees
without locking. It bypasses newlib's usual `__malloc_lock` hooks, which
are themselves empty in this SDK. Merely supplying those hooks does not
protect this allocator.

`Source/WTF/wtf/amigaos3/Malloc.c` supplies wrappers around `_malloc_r`,
`_free_r`, `_calloc_r`, and `_realloc_r`. The shared CMake allocator module
links them with the corresponding GNU ld `--wrap` flags. They serialize
the actual SDK allocator with a recursive Exec semaphore initialized
without allocation. Keep these wrappers and flags in external test
executables as well as the browser.

The native malloc alignment also falls below WebKit's assumed eight-byte
minimum. OS3 `PreciseAllocation` records the exact adjustment to a 16-byte
aligned header, ensuring its cell has the required low-bit tag. Growth,
shrink, failed reallocation, lower-tier reuse and freeing retain the original
allocator base. The engine-linked `odyssey_precise_allocation_test` target
builds `bin/odyssey-precise-allocation-test`; it passed 11,095 native checks
using controlled residues 0, 4, 8 and 12, including payload and canary checks
in `build/amigaos3-precise-allocation-1/output.txt`.

## Garbage collector snapshots

Normal threads, TLS, mutexes and conditions use the repaired SDK pthreads. OS3 stack
bounds are captured by the worker from Exec, and `Thread::create` waits
for that initialization before returning.

The snapshot backend in `amigaos3/ThreadContext.cpp` uses classic Exec's
single-CPU scheduling guarantee. `Forbid` prevents task dispatch while
the caller copies roots, and `Permit` restores dispatch after the last
snapshot. Interrupts remain enabled. One Exec nesting level covers all
target threads, avoiding overflow of Exec's signed-byte nesting counter.
The code cannot allocate, lock, wait or call blocking device functions
inside this interval. Buffer growth happens after threads resume.

A switched-out task's `tc_SPReg` points to the Exec frame containing PC,
SR and D0-D7/A0-A6. The collector scans that frame together with the live
stack. The 16-bit SR and the m68k ABI require scanning every two bytes,
including pointers at addresses two bytes past a four-byte boundary.
The backend rejects missing/exited tasks and checks stack bounds; it
does not report success for a running task or substitute an empty
register snapshot. Destruction clears the task pointer before exit.

Forbid pauses other applications too, so the duration of these bounded
copies needs measurement with real browsing workloads. This backend is
for classic uniprocessor AmigaOS and the 68020 software-floating-point
build, not an SMP suspension implementation.

## Validation boundary

The native WinUAE probes exercise real compiled platform/runtime code:

- `ThreadContextTest.cpp`: suspended D2/A2 roots, word-aligned roots,
  heartbeat stability and restart, more than 128 nested snapshot handles,
  post-exit rejection, and nested once initialization with compiler TLS.
- `RuntimeTest.cpp`: standard threads, mutexes, conditions, local static
  guards, nested once initialization, shared/weak ownership, compiler TLS,
  monotonic sleeps, and concurrent malloc/free.

The combined snapshot and raw-pthread nested-once/TLS test passed with zero
failures and return code 0 in `build/amigaos3-platform-reviewed/output.txt`.
The standard runtime/allocator stress, including actual monotonic sleeps
and the repaired pthread archive, also passed with zero failures and return
code 0 in `build/amigaos3-runtime-conversions-final/output.txt`.
The engine math target `odyssey_math_test` passed all checks in
`build/amigaos3-math-conversions-final/output.txt`.

The native JSC shell passed 2,142 checks, including IEEE arithmetic, BigInt,
typed-array byte order, Float32 rounding, Unicode/ICU and retained objects
across 32 explicit collections, in
`build/amigaos3-jsc-ieee-final/output.txt`. The script is
`Tools/AmigaOS3/jsc-test.js`. Consult the runner output and artifact hashes
rather than treating a successful compilation as a runtime pass. This
establishes a native engine smoke test, not complete GC, browser, or
physical-hardware correctness.
