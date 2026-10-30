# AmigaOS 3 port status

Updated: 2026-10-30.

## Current result

The `AmigaOS3` branch contains a working native 68020/software-floating-point
port of Odyssey 3.2.2. The complete MUI frontend, WebCore, JavaScriptCore,
ICU and native dependencies build and link. The browser runs in an AmigaOS
3.2 WinUAE guest and passes the browser checks below. This is a tested
development port, not a claim of production readiness or physical-hardware
validation.

The base is AmigaLabs tag `odyssey_3.2.2`, commit
`a202d810158265daed814fee7912db434a1ce353`. The older 2.24.4 experiment is
preserved as `AmigaOS3-webkit-2.24.4`; it is not the current port base.
The packaged source revision is
`065f1615ac19c00f72bd2279d6681e940c6d44c1`.

## Build and installation

The matched build uses `/opt/amiga-16.2-20261029` (AmigaDev 16.2-rc21),
the pthread-enabled repaired C++ runtime in
`build/amigaos3-sdk/cxx-runtime-oct29`, and dependencies in
`build/amigaos3-sdk-oct29`. Do not mix the October 14 and October 29 SDK
profiles: their newlib reentrancy layouts differ.

- Build directory: `build/amigaos3-browser-oct29`.
- Local installation archive: `build/Odyssey-AmigaOS3-local.zip`
  (53,073,226 bytes), with a `.sha256` sidecar.
- Archive SHA256:
  `0d249e83821001099741866bb743c853a630d4bd26a58dd27abba06faa1ff888`.
- Packaged OWB SHA256:
  `74b6ade751d6e79d4b81299dbd6632e8b112a95b0c17325f434a5645b4c0ce94`.
- Packaging verification: `build/amigaos3-install-final/verification.json`.
  All 2,589 staged files match the ZIP; CRC and recorded checksums pass.

The archive is local and has not been published as a release. It includes
the browser resources, fonts, notices and build identity; OS, MUI, RTG and
AmiSSL runtime installations are separate prerequisites.

See [installation instructions](Tools/AmigaOS3/INSTALL.txt),
[build instructions](README.AmigaOS3.md),
[SDK profiles](Tools/AmigaOS3/SDK-PROFILES.md), and
[bundle reproduction](Tools/AmigaOS3/INSTALL-BUNDLE.md).

## Validated behavior

These are native m68k emulator results, not host-only unit tests.

| Area | Result and evidence |
| --- | --- |
| JavaScriptCore and ICU | 2,142 checks, zero failures, `RETURN=0`; `build/amigaos3-jsc-oct29-environment/output.txt`. Includes arithmetic, Unicode, regular expressions and repeated garbage collections. |
| Browser rendering | DOM/Unicode, CSS layout, canvas and JavaScript pass in the final browser fixture. |
| HTTPS | Trusted JSON and script loads pass; wrong-host and untrusted certificates are rejected. Certificate verification remains enabled. |
| Concurrent networking | Unicode WebSocket echo, HTTP during a pending TLS connection, cancellation and subsequent HTTP pass. |
| Storage | localStorage survives navigation and a real page reload. |
| Native input | Actual focus, typing `odyssey68k`, and clicking pass with trusted DOM events on the preceding executable (`36bccc32...`). The final executable changes only the disabled networking trace guard. |
| Shutdown | Final executable exits normally through native Right-Amiga Q and its confirmation requester, with `RETURN=0`. |
| Runtime | Focused tests cover tagged-pointer alignment, threading, TLS keys, condition deadlines, allocation/GC, arithmetic, native filesystem operations, graphics and fonts. See the linked runtime and SDK documents for individual test boundaries. |

Final browser evidence is in
`build/amigaos3-browser-release-final/result.json` and `release-proof.json`.
Input evidence is in
`build/amigaos3-browser-network-final/browser-only-proof.json`.
These files bind results to exact binaries and fixture inputs; updating
documentation does not constitute another native test run.

## Requirements and remaining work

The tested configuration uses an AmigaOS 3.2 guest, 68020 without an FPU,
512 MiB Zorro III RAM, 800x600x24 RTG, Zune68 MUI5 21.227 with its matching
zunemaster and Calltips class, registered PNG datatypes, AmiSSL 5.27 and a
native long-filename FFS DOS7 installation. These are tested settings,
not minimum requirements. Ordinary Zune MUI 19.81 did not load the tested
Calltips class.

- Physical Amiga hardware and real TCP/IP stacks remain untested; current
  networking proof uses WinUAE socket emulation and controlled servers.
- Minimum memory, resource consumption and real-world browsing performance
  still need measurement.
- Cold browser restart persistence and quitting with active connections
  have not been established by the final browser matrix.
- Automatic TopSites screenshots are disabled in the tested configuration
  with `Set OWB_DISABLE_TOPSITES_SCREENSHOTS 1`; synchronous screenshot
  rendering otherwise delays network callbacks. History remains enabled.
- DNS resolution remains synchronous. Audio/video playback and screen
  blanker inhibition are unavailable in this build.
- The SDK still has broader threaded-newlib integration work, including
  shared libc `errno` and allocator synchronization. Odyssey carries
  focused runtime repairs and task-owned socket/AmiSSL errors; this does
  not make every use of newlib reentrancy state thread-safe.
- Odyssey 3.2.2 uses an older WebKit lineage. This port does not establish
  current upstream WebKit security coverage.

## Upstream submissions

PR preparation is separate from the packaged browser. The package retains
its tested repairs until appropriate upstream versions are rebuilt and
validated together.

- curl: [use the native classic-Amiga socket error channel](https://github.com/curl/curl/pull/23695).
  Current-source host build, lint and 79 unit tests pass; native current
  socket sources compile. Both upstream AmigaOS m68k/AmiSSL CI builds
  (CMake and autotools) also pass. The recorded Amiga runtime regression is against
  curl 8.23.0 and demonstrates receive-error misclassification before the
  repair and `CURLE_AGAIN` afterward.
- AmigaPorts/newlib-cygwin:
  [#33: determine `isatty` from the DOS handle](https://github.com/AmigaPorts/newlib-cygwin/pull/33).
  Fresh native regression passes all nine checks; the unchanged SDK fails
  six. The fix handles redirected streams, invalid descriptors and actual
  interactive handles rather than assuming descriptors 0 through 2 are TTYs.
- AmigaPorts/newlib-cygwin:
  [#34: keep public division functions out of compiler helpers](https://github.com/AmigaPorts/newlib-cygwin/pull/34).
  Removes duplicate `div`/`ldiv` aliases while retaining the compiler helper
  entry points. A private archive rebuilt from current upstream sources
  passes 45 native division/ABI checks; the original duplicate-symbol link
  failure is preserved. These focused tests are not a full newlib rebuild.
- AmigaPorts/aros-stuff:
  [#9: initialize and lock TLS key table lookups](https://github.com/AmigaPorts/aros-stuff/pull/9).
  Native allocation, set-specific and deletion regressions pass with the
  repair. Controlled nonzero allocation reproduces exhaustion after six
  of 64 keys with the unchanged SDK; forced relocation exposes the stale
  table-pointer race. These tests establish valid failing interleavings,
  not their frequency during normal browsing.
- AmigaPorts/aros-stuff:
  [#10: normalize absolute condition wait intervals](https://github.com/AmigaPorts/aros-stuff/pull/10).
  Signed interval arithmetic fixes cross-second subtraction and expired
  deadlines; invalid nanoseconds are rejected. All 16 native checks pass,
  including mutex reacquisition. The unchanged SDK fails the bounded
  watchdog regression. Both pthread PRs also pass the five-multilib build
  checks; C++ tests are skipped with the stock non-threaded libstdc++.

The broader arithmetic, allocator, environment-startup and threaded-runtime
findings remain documented in [the upstream audit](Tools/AmigaOS3/UPSTREAM.md).
Some older findings are already fixed upstream; that document separates
those from remaining defects and configuration choices. Valid m68k ABI
properties and WebKit-specific alignment/threading adaptations belong in
the port rather than being reported as compiler defects.
