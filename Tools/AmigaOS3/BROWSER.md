# Browser build bring-up

After building ICU, the pthread C++ runtime and the native dependencies,
configure the complete browser source graph with the staged prefix:

```sh
cmake -S . -B build/amigaos3-browser -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/Source/cmake/AmigaOS3.cmake" \
  -DAMIGAOS3_DEPENDENCY_ROOT="$PWD/build/amigaos3-sdk" \
  -DAMIGAOS3_RUNTIME_ROOT="$PWD/build/amigaos3-sdk/cxx-runtime" \
  -DCMAKE_BUILD_TYPE=Release -DPORT=MUI
cmake --build build/amigaos3-browser --target WebCore -j4
```

This configures successfully against actual m68k archives. Target
pkg-config metadata is isolated from the host. Static dependency edges
include pixman, Expat, sharpyuv and ICU; AmiSSL is represented by its real
SDK stubs and explicit native task lifecycle, without libamisslauto or
artificial libssl/libcrypto aliases. Video and related media features
default off until a native
decoder stack is available.

The complete WebCore static archive now builds successfully for the native
target. `build/amigaos3-browser/lib/libWebCoreMUI.a` contains 740 native Amiga
object members (76,962,328 bytes). The final build log is
`build/amigaos3-browser/webcore-build-final-incremental.log`; member formats,
size and SHA256 are recorded in `build/amigaos3-browser/webcore-archive.json`.
The pass includes the latest curl shutdown lifecycle and libxml2 2.12 const
error-callback adaptation. PAL and JavaScriptCore prerequisites also compile.

This remains an incomplete browser port. WebKitLegacy's MUI application
still has separate OpenURL SDK integration, RTG alpha-blit and startup
`kprintf`/`DumpTaskState` gaps before a complete executable can be linked and
tested. Cursor routing, keyboard conversion and clipboard notification
adapters have focused coverage described below. Archive compilation does
not establish browser rendering, navigation or complete network behavior.

Run one Ninja process per build directory. This local build's dependency
log needed recovery after overlapping invocations. The final archive build
passed, but the next build may conservatively recompile objects; incremental
rebuild behavior was not validated. Generated bookkeeping diagnostics are
preserved under `build/amigaos3-browser/ninja-bookkeeping`.

## Persistence ABI regression

The SDK defines int32_t as long, distinct from its equally wide int.
Persistence now maps native int/unsigned to the existing fixed-width
encoding, including the checksum's type salt. Its arithmetic SFINAE test
also now examines the actual type instead of the remove_const trait.

```sh
cmake --build build/amigaos3-browser --target amigaos3-persistence-test -j4
```

Run `build/amigaos3-browser/bin/amigaos3-persistence-test` through
`run-platform-test.py`, with success marker
`PASS native persistence integer format`. The CMake-built executable
passed on the AmigaOS 3.2 guest in
`build/amigaos3-persistence-cmake/output.txt`. It compares encoded bytes
and checksums against int32_t/uint32_t, round-trips INT_MIN, INT_MAX and
UINT_MAX, and rejects a truncated integer. This is native guest evidence,
not a physical-hardware or full-browser result.

## MUI input and clipboard

The production keymap.library keyboard adapter and shared ICU local
charset conversion pass the isolated native keyboard test, including
dead-key composition; see [KEYBOARD.md](KEYBOARD.md) for coverage and
reproduction.

The native MUI probe exercises the production custom-class dispatcher,
DoSuperNew tag varargs, MUI_HOOK and HookEntry bridges. CursorMorphOS maps
WebCore cursor types to MUIA_PointerType, and WidgetMorphOS routes changes
to the active browser area. The probe verifies attribute storage and
MUIM_WhichPointerType; it does not verify rendered pointer images. Current
zune68 updates the window pointer before forwarding IDCMP_MOUSEMOVE, while
setting MUIA_PointerType only changes the stored value. Thus a pointer
change may appear on the next mouse event and a stationary CSS cursor
change may remain stale. No external zune68 source was changed.

On classic AmigaOS, clipboard.device invokes CBD_CHANGEHOOK synchronously
in the writer's task under its change semaphore and Forbid. The OS3 hook
therefore only marks a pending change and signals a preallocated owner
signal. A MUI Application input handler consumes it on the browser main
task, including nested NewInput loops. Device hook removal completes
before the input handler and signal are released. Pending callbacks cannot
signal a reused bit after shutdown. Hook-install and signal-allocation
failures clean up their resources.

Local writes drain notifications after IFF/device I/O has returned and
before Pasteboard stores new rich content. Copy/paste creation also drains
pending changes before consulting the cache, since MUI may dispatch a
keyboard event before a signal handler. These boundaries preserve the
existing cache invalidation ordering without entering WebCore inside the
device callback.

`odyssey-platform-test` includes the shared production notification helper
and uses real clipboard.device writes from pthread workers. It checks
coalescing, delivery only on the owner task through MUI NewInput, signal
mask registration, local synchronous draining, hook removal, signal reuse
and allocation failure. The native guest run is recorded in
`build/amigaos3-clipboard-notification-2/output.txt` (0 failures, RETURN=0).
The probe overwrites the disposable guest clipboard. The production
clipboard and Pasteboard translation units compile; appclass integration
still encounters the separate missing OpenURL SDK header. Full-browser
copy/paste and actual rendered cursor behavior remain unverified.
