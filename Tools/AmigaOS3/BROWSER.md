# Browser build bring-up

After building ICU, the pthread C++ runtime and the native dependencies,
configure the complete browser source graph with the staged prefix:

```sh
cmake -S . -B build/amigaos3-browser-oct29 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/Source/cmake/AmigaOS3.cmake" \
  -DAMIGAOS3_TOOLCHAIN_ROOT=/opt/amiga-16.2-20261029 \
  -DAMIGAOS3_DEPENDENCY_ROOT="$PWD/build/amigaos3-sdk-oct29" \
  -DAMIGAOS3_RUNTIME_ROOT="$PWD/build/amigaos3-sdk/cxx-runtime-oct29" \
  -DCMAKE_BUILD_TYPE=Release -DPORT=MUI
cmake --build build/amigaos3-browser-oct29 --target OWB -j4
```

This configures successfully against actual m68k archives. Target
pkg-config metadata is isolated from the host. Static dependency edges
include pixman, Expat, sharpyuv and ICU; AmiSSL is represented by its real
SDK stubs and explicit native task lifecycle, without libamisslauto or
artificial libssl/libcrypto aliases. Video and related media features
default off until a native
decoder stack is available.

The original SDK profile's complete WebCore archive built successfully.
`build/amigaos3-browser/lib/libWebCoreMUI.a` contains 740 native Amiga
object members (76,962,328 bytes). The final build log is
`build/amigaos3-browser/webcore-build-final-incremental.log`; member formats,
size and SHA256 are recorded in `build/amigaos3-browser/webcore-archive.json`.
That historical pass includes libxml2 2.12 const error-callback adaptation.
The current matched October 29 build uses the separate directory above and
includes PAL, JavaScriptCore, WebCore and the complete MUI frontend.

The MUI frontend now has native
callback bridges, an optional classic OpenURL binding and an RTG alpha
fallback. The alpha implementation passes actual RTG pixel readback and
layer-clipping checks; see [GRAPHICS.md](GRAPHICS.md). The complete executable now links as
`build/amigaos3-browser-oct29/bin/OWB`; the successful link is recorded in
`build/amigaos3-browser-oct29/browser-relink.log`. The earlier
`browser-build.log` ends at the subsequently repaired SDK integer-alias
link failure. The native browser
launch in `build/amigaos3-browser-launch-2` passes all five page assertions:
DOM/Unicode, CSS layout, canvas pixels, JavaScript and localStorage. Its
`live-inspect.png` screenshot was visually inspected and shows the rendered
page and passing result. This uses the AmigaOS 3.2 guest, Zune68/MUI,
Picasso96 and a native DOS7 data volume. The later
`build/amigaos3-browser-navigation-60s/result.json` records verified HTTPS
content and script execution, rejection of wrong-host and untrusted
certificates, and localStorage retained across navigation and a real reload.
The basic-page run in `build/amigaos3-browser-quit-1` also exits through the
real Quit confirmation with `RETURN=0` and `BROWSER_EXIT` in
`system/browser-output.txt`. Its GDB input logs and screenshots preserve
that interaction. These results apply to their recorded binary hashes;
The final `build/amigaos3-browser-release-final/result.json` also passes
Unicode WebSocket echo, HTTP while TLS remains pending, cancellation,
subsequent HTTP, verified HTTPS, navigation/reload storage, and normal Quit
with `RETURN=0`. Tracing is disabled. Its OWB SHA256 is
`74b6ade751d6e79d4b81299dbd6632e8b112a95b0c17325f434a5645b4c0ce94`.
Automatic TopSites screenshots are disabled as documented in INSTALL.txt;
their synchronous work otherwise delays network callbacks.
The preceding `browser-network-final` run proves native page focus,
typing and clicking through trusted DOM events, and also exits normally.
The final binary differs only in the disabled tracing path, with no
frontend changes. Shutdown during an active connection remains a separate
case. See [BROWSER-FIXTURE.md](BROWSER-FIXTURE.md).
Cursor routing, keyboard conversion and clipboard notification
adapters have focused coverage described below. Archive compilation does
not establish browser rendering, navigation or complete network behavior.

Run one Ninja process per build directory. A preserved dependency-log snapshot
has an invalid complete path record at byte 3,768,112: its ID is 10203 but
10206 is required. Ninja 1.13.2 advances its recovery offset before checking
that ID, so recovery retains the bad record and loses later dependency
information on each invocation. This explains repeated full recompiles even
with a single writer after the original overlapping invocations. See
`build/amigaos3-probes/ninja-deps-audit.json` and the accompanying snapshot.
If reusing that damaged historical build directory, first stop its writer,
preserve and move the damaged `.ninja_deps` aside, then run one complete
build to regenerate it. Never move or truncate the log while Ninja is
running. The recovery offset behavior is in
[Ninja's dependency-log reader](https://github.com/ninja-build/ninja/blob/v1.13.2/src/deps_log.cc).

## Persistence ABI regression

The SDK defines int32_t as long, distinct from its equally wide int.
Persistence now maps native int/unsigned to the existing fixed-width
encoding, including the checksum's type salt. Its arithmetic SFINAE test
also now examines the actual type instead of the remove_const trait.

```sh
cmake --build build/amigaos3-browser-oct29 --target amigaos3-persistence-test -j4
```

Run `build/amigaos3-browser-oct29/bin/amigaos3-persistence-test` through
`run-platform-test.py`, with success marker
`PASS native persistence integer format`. The earlier profile's CMake-built
executable passed on the AmigaOS 3.2 guest in
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
clipboard, Pasteboard and application integration translation units compile. Full-browser
copy/paste and actual rendered cursor behavior remain unverified.
