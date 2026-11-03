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

## Resume here

Keep Odyssey 3.2.2 as the base; the decision is to port its platform layer,
not restart on 2.24.4. Start with `git status --short` and `git log -5
--oneline` in `/Users/stepan/git/OdysseyWebBrowser`. Preserve unrelated work.
The package above remains tied to its recorded source revision; later
documentation commits and upstream PR updates do not change its identity.

For an incremental rebuild using the existing matched dependencies:

```sh
cmake -S . -B build/amigaos3-browser-oct29 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/Source/cmake/AmigaOS3.cmake" \
  -DAMIGAOS3_TOOLCHAIN_ROOT=/opt/amiga-16.2-20261029 \
  -DAMIGAOS3_DEPENDENCY_ROOT="$PWD/build/amigaos3-sdk-oct29" \
  -DAMIGAOS3_RUNTIME_ROOT="$PWD/build/amigaos3-sdk/cxx-runtime-oct29" \
  -DCMAKE_BUILD_TYPE=Release -DPORT=MUI
cmake --build build/amigaos3-browser-oct29 \
  --target OWB jsc odyssey_tagged_pointer_test -j4
```

Only one process/agent should write this build directory. Use a fresh build
directory when changing SDK profiles. The `/opt/amiga-16.2` symlink has
changed during development; use the physical SDK path above. MUI headers
are already in the SDK; do not add Zune68's AROS source include tree.

Read the following documents for the area being changed:

- [SDK-PROFILES.md](Tools/AmigaOS3/SDK-PROFILES.md): rebuild order and exact
  commands for the C++ runtime, ICU and other dependencies.
- [RUNTIME.md](Tools/AmigaOS3/RUNTIME.md) and
  [ICU.md](Tools/AmigaOS3/ICU.md): runtime repairs, threading/GC and Unicode.
  Native WTF adapters are under `Source/WTF/wtf/amigaos3`; runtime patch
  recipes are in `Tools/AmigaOS3/build-cxx-runtime.py`.
- [BROWSER.md](Tools/AmigaOS3/BROWSER.md) and
  [BROWSER-FIXTURE.md](Tools/AmigaOS3/BROWSER-FIXTURE.md): browser build,
  installed guest prerequisites, launch commands, input and network tests.
  To match the final matrix, use `--disable-topsites-screenshots` and leave
  `--trace-streams` off. Use a new output directory and unused ports.
- [CURL.md](Tools/AmigaOS3/CURL.md) and
  [SOCKET_ERRNO.md](Tools/AmigaOS3/SOCKET_ERRNO.md): socket ownership,
  error isolation and the deterministic receive regression.
- [UPSTREAM.md](Tools/AmigaOS3/UPSTREAM.md): established SDK defects,
  already-fixed findings and candidates still needing evidence.

The local guest fixture is `build/amigaos3-amissl-fixture`; the ROM is
`~/AmigaOS/AmigaOS-3.2-full/ROM/kicka1200.rom`, the emulator is
`~/git/WinUAE_mac/WinUAE/bin/winuae`, and MUI5 runtime builds are under
`~/git/zune68/build/muimaster-mui5`. Control guest input through the native
GDB/monitor backend, not host keyboard or terminal injection. Check for
active runners before choosing ports or changing a fixture.

The `build/` archives, installed guest, test outputs and `/private/tmp`
upstream checkouts are local artifacts, not a portable source checkout.
A new machine needs the matching SDK and separately installed OS/MUI/AmiSSL
assets, followed by the documented builds and fresh tests. Do not claim
the recorded native passes for a newly built executable without rerunning
the relevant matrix and recording its hash.

Suggested next validation order:

1. Cold-restart persistence and shutdown with active HTTP/WebSocket/TLS
   connections, extending the existing native browser fixture.
2. Real TCP/IP stack and physical-hardware testing when those systems are
   available, repeating the memory measurements there.
3. Broader current-SDK allocator and threaded-newlib regressions before
   replacing the local runtime repairs or proposing broader upstream fixes.

Commit messages use Linux kernel style, all lines at most 72 characters,
with no Signed-off-by tags; explain what changed and how. The user has
explicitly asked not to add AI-assistance disclosures to submissions.
Review comments should be read literally: apply an accepted code suggestion
verbatim and verify its text before pushing. Do not post replies or merge
PRs without authorization.

## Build and installation

The matched build uses `/opt/amiga-16.2-20261029` (AmigaDev 16.2-rc21),
the pthread-enabled repaired C++ runtime in
`build/amigaos3-sdk/cxx-runtime-oct29`, and dependencies in
`build/amigaos3-sdk-oct29`. Do not mix the October 14 and October 29 SDK
profiles: their newlib reentrancy layouts differ.

- Build directory: `build/amigaos3-browser-oct29`.
- Amiga archive: `build/amigaos3-browser-oct29/Odyssey-AmigaOS3.lha`
  (47,705,765 bytes), from the `lha` target, with a `.sha256` sidecar.
  This is the archive to use on an Amiga.
- LHA SHA256, built from `f5565331ed`:
  `de0ae655ba67f3741e071eeabd3ee99f4afe6c4f8ef2e8ca2baea76e5d4a4aca`.
  `Build/inputs.json` records the checkout's state, so rebuilding after any
  commit or tracked edit changes this hash, though not the packaged OWB.
- Packaged OWB SHA256:
  `8053c28aca69b4173a5321cf92087dd1619b17bbce76ce70e51b380708df08c2`.
- Previous archives, both with OWB `74b6ade7`: the local ZIP
  `build/Odyssey-AmigaOS3-local.zip`, not rebuilt since, and LHA `5b12cc5b`.
  Native LhA 2.15 extraction was proved only on that LHA, in
  `build/amigaos3-lha-final`, where all 253 files and the drawer icon matched
  with `RC=0`. Repeating it needs a native `lha_68020`, which is no longer in
  the scratch area.
- Packaging verification of the previous ZIP:
  `build/amigaos3-install-final/verification.json`. All 2,589 staged files
  matched; CRC and recorded checksums passed.

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
native long-filename FFS DOS7 installation. Memory requirements are in
[MEMORY.md](Tools/AmigaOS3/MEMORY.md): 256 MiB is recommended with at least
150 MiB free at launch, and 128 MiB froze on a heavy page, measured on Zune68.
The browser requires muimaster.library 21 and reports the version it found;
ordinary Zune 19.81 and MUI 3.9 cannot run it. Earlier Zune68 releases on
Aminet were built from `main` and report 19.81. Zune68 Beta 2 (2026.11.02)
reports 21.227 and ran the corpus with only its own classes, including its
Calltips.mcc 20.12 (`fpu-corpus-zune1102-all`).

On an A4000 layout (68040, 128 MiB processor-slot and 256 MiB Zorro III RAM),
launched from a 4 KiB stack, OWB `8053c28a` visited all six corpus pages and quit
natively with `RETURN=0` on Zune68 and on genuine MUI 5.0-20210831 (21.227). From
a Workbench icon it opens on genuine MUI, quits and frees its memory, and on Zune
19.81 it shows the version requester and exits. Results are in
`build/amigaos3-crash2`: `cq-zune`, `cq-mui5`, `wb6-mui5` and `wb7-z19`. Run
genuine MUI with `--mui-package`.

The tester's `#80000008` was an icon launch on Zune 19.81 or MUI 3.8. On a
Workbench start `main` reopens stderr on `NIL:`, and newlib's first write calls
`ExamineFH` on it to size a buffer, which crashes because `NIL:` has no handler.
Both streams are now unbuffered. The SDK's `_fstat` still has this fault for any
buffered stream on `NIL:`, which belongs in newlib.

Quitting after any page which created a `WorkQueue`, such as one with a `data:`
URL, used to hang in the SDK's exit join with about 92 MiB still allocated. The
earlier Quit proofs used a test page without one. Queue threads now stop at exit.

Pages used to stay blank on any screen of 8 bits or fewer, which looked like a
fault in the `about:` page. CyberGraphX does not reduce ARGB to pens, and the
fixture missed this because it forces a 24-bit screen. These screens now use a
dithered pen cube, described in [GRAPHICS.md](Tools/AmigaOS3/GRAPHICS.md). OWB
`057dd6cb`, which the archive above does not contain yet, showed `about:` on
8-bit RTG on Zune68 and on a 16-color native screen, scrolled and quit natively on
8-bit on genuine MUI with `RETURN=0`, and passed the 24-bit fixture from a 4 KiB
launch. The previous build, `21383ae1`, showed `about:` on genuine MUI at 8 and 24
bits, and the test page rendered as before at 16 and 24 bits. It differs only by
an alpha check and one message. A true 256-color native screen was not reached:
asking for 8 planes gave a 16-color screen. Native planar writes at 8 planes
pass in `odyssey-graphics-test`, with 133,465 checks and no failures. Startup
failures now show a requester. Icon starts proved this on the previous build,
`21383ae1`, with no TCP/IP stack, no Calltips.mcc, and test patches of the binary
which made cybergraphics.library and the stack allocation fail. The Calltips.mcc
case was repeated on `057dd6cb`, whose text differs. Each run exited after the
requester. Results are in `build/amigaos3-crash2`: `e1-*`, `f2`-`f5` and `g1`-`g6`.
The failure to create the application object was not exercised.

The source, not yet packaged, now builds for 68020 to 68060
with an FPU (`-m68020-60 -m68881`) at `-O2`, with reduced ICU data, a new
heap, smaller caches and a stripped staged executable. OWB `57f46e57` in
`build/amigaos3-browser-fpu` needs an FPU and says so without one. It peaked at
106.4 MiB rather than 135.5, took 15 to 43% less time per page on the
cycle-exact 68030, passed both visits held to 128 MiB, and ran the corpus on a
68040 with the Workbench 3.2 SetPatch and 68040.library. The page checks
also pass on a 68060 with 68060.library. On a 68LC040 the Workbench 3.2 CPU
libraries do not emulate the missing FPU; SoftIEEE from Aminet should, but
that is untested and slow. An
`-Os` build in `build/amigaos3-browser-fpu-os` saves another 17 MiB but is as
slow as the software floating point build. Results and open decisions are in
[MEMORY.md](Tools/AmigaOS3/MEMORY.md#results-of-the-changes). The native runners
now set WinUAE's `fpu_strict=true`.

- Favicons and activity animations are not drawn at 8 bits or fewer, because
  the alpha path cannot blend with pens.
- The fixture only tests a 24-bit screen. On it, Intuition's close, depth,
  zoom and size gadgets and the scroll arrows are missing from every window,
  including a clean Workbench 3.2 with its own Picasso96. That WinUAE build's
  Unix RTG code runs `BlitPlanar2Direct`, but the gadgets' planar images never
  reach the frame buffer. The same image on an 8-bit RTG or native screen
  draws them.
- Physical Amiga hardware and real TCP/IP stacks remain untested; current
  networking proof uses WinUAE socket emulation and controlled servers.
- When memory runs out or a fatal check fails, OWB now shows a requester
  ("Odyssey ran out of memory and has to quit.") and exits with 20, instead
  of stopping silently. Four 80 MiB runs on the 68040 showed the requester;
  the last shut down normally, and one ran out of memory again during
  shutdown and still exited. In that case MUI's application object can stay
  behind. Caches are still released only after an allocation fails; see
  MEMORY.md. Google's sign-in page had stopped OWB through a vtable check
  that is now disabled. The page now loads, but on the emulated 68040 each
  key press runs Google's script for about 20 s. Two causes of that are
  fixed: symbol table locks that each cost two atomic library calls, and a
  sloppy `eval` on the page that sent every closure variable read below an
  `eval` through a full scope chain search (4.3 times slower in a `jsc`
  loop). The key press time has not been measured again.
- GitHub's style sheets wrapped in `@layer` are dropped: cascade layers are
  experimental and off in this WebKit, and its CSSOM has no wrapper for layer
  rules. Such sheets are now cached like others, which brought the local
  GitHub snapshot from 109.1 to 103.4 s, but their styles still do not apply.
- Zune68 before Beta 2 drew the status bar's progress text past the gauge
  when it was wider, so each update overdrew the previous one ("69 B of
  0000000000ddddd" in `fpu-corpus-scopefix`). Beta 2 (2026.11.02) clips the
  text to the gauge, as MUI 3.9 does: on the same 68040 corpus with only its
  libraries replaced, the gauge shows "69 B of" and nothing beyond it, with
  unchanged page times (`fpu-corpus-zune1102`). That fixture has no Lamp.mcc.
  With all of Zune68's classes the network lamps appear and the gauge shows
  "69 B of 69 B read" in full (`fpu-corpus-zune1102-all`).
- Live-Internet performance has not been measured.
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
  As checked on 2026-10-30, the PR is open at `7d6febdbe2fc677bf5f37d861190ae2b7be5307e`.
  The reviewer said "LGTM now!" after the exact formatting suggestion was
  applied, and both formatting threads were resolved. The build results
  above precede those whitespace-only follow-ups; do not assume every
  later CI run has completed. The local PR checkout is
  `/private/tmp/odyssey-curl-socket-errno`, branch
  `amiga-native-socket-errno`, with the user's fork remote named `fork`.
  Refresh GitHub state before further action; review acceptance is not a
  merge, and the packaged curl archive has not been replaced.
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
