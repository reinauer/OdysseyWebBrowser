# Native browser launch fixture

`run-browser-test.py` builds a disposable copy of an installed AmigaOS 3
system. It copies the repository's `Dist` assets and the dependency prefix's
relocatable fontconfig configuration and Noto Sans font into `SYS:Odyssey`.
It never installs anything into the source system, compiler, or zune checkout.
Installed OS, MUI, Picasso96 and AmiSSL files remain local test inputs; this
workflow does not redistribute them.

Use a native FFS hardfile for browser data. WinUAE's directory-backed host
filesystem has a separately observed record-locking defect; keeping the
browser databases on that shared folder would distort SQLite testing.
Create a blank 256 MiB DOS7 FFS image with long filenames using amitools:

```sh
mkdir -p build/amigaos3-browser-support
"$HOME/Library/Python/3.14/bin/xdftool" \
  build/amigaos3-browser-support/browser-data-256m-dos7.hdf \
  create chs=16384,1,32 + format OdysseyData DOS7 + info
```

`--data-hdf` clones this input to the new run directory and mounts the clone
as `DH1:` (32 sectors, one head, two reserved 512-byte blocks, nonbootable).
`--xdftool` populates only that clone with `Odyssey`; the input image must
not already contain an Odyssey directory. This avoids copying tens of
megabytes through the guest on each iteration. The guest verifies the real
mounted DOS7 type and size, writes/flushes/reads/deletes a filename longer
than 30 characters, and reads the staged font. It then launches
`DH1:Odyssey/OWB`, making `PROGDIR:` and its databases native FFS paths.
The original image remains unchanged and its hash is recorded.

The source system needs AmigaOS 3.1 or later, installed Picasso96 with the UAE
monitor, `C:LoadMonDrvs`, `C:IPrefs`, `C:LoadWB`, and AmiSSL. The existing local
`build/amigaos3-amissl-fixture` provides these. The runner enables WinUAE's
Zorro III RTG card, native socket emulation and 512 MiB Zorro III RAM. A native
helper chooses an actual available 800x600, 24-bit CyberGraphX mode and writes
its ScreenModePrefs into the disposable `ENV:` before IPrefs and Workbench
start. It checks the resulting bitmap depth and CyberGraphX identity.
The installed PNG datatype descriptor, `png.datatype` and its
`picture.datatype` superclass are required inputs. After datatype
registration, the helper opens an actual MUI application/window/group with
a Dtpic child loading the staged `Resource/back.png`. The child must open
successfully and measure exactly 26x21 pixels, matching the PNG header.
This exercises the same picture-loading and layout path as toolbar buttons.

When installed, `C:AddDataTypes REFRESH QUIET` runs before IPrefs, as in a
normal desktop startup. The existing wallpaper preference is preserved;
without datatype initialization its otherwise present ILBM image produced
a blocking requester. Only fixtures lacking AddDataTypes omit that
wallpaper preference. The inherited ToolManager auto-dock also references
images on an unmounted `Work:` volume. When those references exist, only
`WBStartup/ToolManager` and its icon are moved out of the disposable startup
directory. Other utilities and preferences remain intact. All omissions are
backed up under `omitted-desktop-files` and recorded with hashes and reasons
in `inputs.json`; the source installation is unchanged. Fixture-only runs
close the boot console after their checks so screenshots show Workbench.
`build/amigaos3-browser-fixture-desktop-visible/screen.png` shows the
requester-free desktop; the background is plain gray, so this evidence
does not establish wallpaper rendering. The focused native Dtpic check
passed in `build/amigaos3-browser-fixture-dtpic-2/fixture-output.txt`.

Use the matching MUI5 development variants from zune68:

- `build/muimaster-mui5/muimaster.library` reports version 21.227.
- `build/muimaster-mui5/zunemaster.library` is required by its native
  `Listtree.mcc` and other external Zune classes.
- The ordinary `build/muimaster/muimaster.library` reports 19.81 and cannot
  load the available Calltips class. Do not substitute it for this fixture.

The local AmigaOS 3 MUI package supplies a genuine m68k Calltips class. The
zune68 source tree has a Calltips implementation, but its current native
build does not install a Calltips binary. Extract the existing packaged
class into ignored build storage:

```sh
mkdir -p build/amigaos3-browser-support
bsdtar -xf "$HOME/Downloads/MUI-5.0-20210831-os3.lha" \
  -C build/amigaos3-browser-support MUI/Libs/mui/Calltips.mcc
```

The runner verifies HUNK headers for the browser, MUI libraries and supplied
classes; it does not accept the PPC OS4 package as an alternative. The native
probe then loads the actual Application, Window, Group, String, List, Title,
Listtree and Calltips superclasses. A file-presence check alone is insufficient.

```sh
python3 Tools/AmigaOS3/run-browser-test.py \
  --system build/amigaos3-amissl-fixture \
  --rom "$HOME/AmigaOS/AmigaOS-3.2-full/ROM/kicka1200.rom" \
  --emulator "$HOME/git/WinUAE_mac/WinUAE/bin/winuae" \
  --mui-library "$HOME/git/zune68/build/muimaster-mui5/muimaster.library" \
  --zune-library "$HOME/git/zune68/build/muimaster-mui5/zunemaster.library" \
  --calltips build/amigaos3-browser-support/MUI/Libs/mui/Calltips.mcc \
  --data-hdf build/amigaos3-browser-support/browser-data-256m-dos7.hdf \
  --xdftool "$HOME/Library/Python/3.14/bin/xdftool" \
  --binary build/amigaos3-browser-oct29/bin/OWB \
  --sdk build/amigaos3-sdk-oct29 \
  --output build/amigaos3-browser-launch-1 \
  --gdb /opt/amiga-16.2/bin/m68k-amigaos-gdb \
  --gdb-port 2350 --http-port 18788 --timeout 600
```

Select the dependency prefix corresponding to the browser's actual compiler
profile. `--prepare-only` stages files without launching; `--fixture-only`
checks the screen and classes without requiring a browser binary. Every
output directory must be new. `--mui-classes` optionally overlays an explicit
native MCC directory. Existing native classes in the input system are
otherwise retained. All staged files, compiler inputs, emulator, ROM, config,
and test HTML are hashed in `inputs.json`.

The browser opens a loopback HTTP fixture. Its JavaScript reports independent
DOM Unicode, CSS layout, canvas pixel, JavaScript and localStorage checks.
`result.json` records these assertions and HTTP requests; it reports failure
if the browser exits, the fixture fails, or the deadline expires. Serving a
page or keeping an emulator alive is not treated as success. This HTTP test
does not validate HTTPS trust, interaction, persistence after restarting,
clean shutdown, or physical hardware. No browser test result is claimed
until a linked browser actually produces the report.

Add `--websocket` to require an actual WebSocket upgrade and Unicode text
echo. The same test opens `wss://127.0.0.1:18789/pending`, whose TCP listener
accepts and records TLS bytes but deliberately never answers. While that
TLS connection is pending, ordinary HTTP and the normal WebSocket must
continue. The page closes the pending connection, waits for its close event,
and fetches another HTTP marker. Passing requires the page assertions and
host evidence of the pending TLS bytes and subsequent TCP close. Ports are
configurable through `--http-port` and `--blackhole-port`. This cancellation
test does not validate certificate trust or the separately synchronous DNS
resolver; it uses only numeric loopback addresses. An independent Node
WebSocket client passed the host echo/close check. The repaired native
browser also passed this sequence without stream tracing in
`build/amigaos3-browser-network-final`.

Add `--https` for verified browser HTTPS requests. The runner creates a
two-day private test CA, a CA-signed `localhost` SAN server certificate,
and an unrelated untrusted `localhost` certificate. Only the copied guest's
`AmiSSL:Certs` receives the CA, under its OpenSSL subject-hash filename;
private keys remain in the run directory. Ports 18444 and 18445 serve the
trusted and untrusted endpoints (`--https-port`/`--untrusted-port`). No
verification flags are disabled and no certificate prompts are accepted.

`--network-timeout` bounds HTTPS requests, the WebSocket echo/cancellation
sequence, and host TLS/WebSocket operations (60 seconds by default).
WebSocket reports retain the timeout reason and timestamp creation, open,
receive, cancellation, error and close events. Host events carry timestamps
too. TLS events record elapsed handshake time. The original 15-second
fixture deadline expired during the first cold TLS request in
`build/amigaos3-browser-navigation-final`; the following trusted script and
both certificate-negative requests completed. A timeout is always a failure,
including in the negative certificate checks.

Keep the runner alive with `--hold 1800` for native input through WinUAE's
GDB backend. Detaching immediately cancels queued input, so resume while
the connection remains open. The following tested commands press and
release Right-Amiga Q, run for three seconds, then disconnect. Numeric
sequence fields use WinUAE's hexadecimal syntax. Use a separate bounded
attach to inspect completion; issuing `interrupt` followed immediately by
another command races GDB's asynchronous stop handling.

```sh
/opt/amiga-16.2/bin/m68k-amigaos-gdb -q -nx -batch \
  -ex 'set confirm off' -ex 'set remotetimeout 5' \
  -ex 'target remote 127.0.0.1:2350' \
  -ex 'monitor input sequence 0 0 0 67 1;2 0 0 10 1;4 0 0 10 0;2 0 0 67 0' \
  -ex 'continue &' -ex 'shell sleep 3' -ex 'disconnect'
/opt/amiga-16.2/bin/m68k-amigaos-gdb -q -nx -batch \
  -ex 'set remotetimeout 5' -ex 'target remote 127.0.0.1:2350' \
  -ex 'monitor input status' -ex 'detach'
```

Input completion proves delivery into the guest input system. Verify the
resulting application behavior separately from screenshots and the guest's
normal browser-exit marker; runner termination is not clean-shutdown proof.

The final alignment build (`de928dcc15adb4de4ab39a223f1d4979494fed9d4b52c71f8af2a909540592c9`)
passed all five basic page checks and normal native Quit in
`build/amigaos3-browser-quit-1`. Right-Amiga Q opened the real confirmation
requester; clicking its Yes gadget through GDB guest input closed Odyssey.
`system/browser-output.txt` records `RETURN=0` and `BROWSER_EXIT` before any
emulator termination. `after-quit.png`, `after-yes.png` and the input/status
GDB logs preserve the interaction. Input completed with no held keys or
buttons. This does not prove shutdown with active WebSocket connections.

For network diagnostics, `--trace-streams` stages
`OWB_CURL_STREAM_TRACE=SYS:browser-trace.txt` in the copied guest and captures
connect/send/receive results in that explicit file. Native worker stdout
can remain bound to its own console despite the parent's shell redirection.
`--disable-topsites-screenshots` stages
`OWB_DISABLE_TOPSITES_SCREENSHOTS=1` for a deliberate A/B run excluding
synchronous thumbnail generation. Both settings are recorded in the input
manifest and leave the source system unchanged. The HTTP-while-TLS-pending
assertion checks that the pending WebSocket is still CONNECTING when the
HTTP callback runs; a late callback after TLS failure cannot pass it.

`--interaction` navigates to a final native-input page after the automated
checks (and after navigation/reload when selected). Focus its field through
native mouse input, type `odyssey68k`, and click Check. The page reports its
actual value plus trusted focus, keydown, input and click events. No script
sets the field value or dispatches input events. The runner requires this
separate interaction report before passing. The native run
`build/amigaos3-browser-network-final` passed with the exact value,
trusted focus, ten keydown/input transitions and a trusted Check click.
Its `interaction-passed.png` and native input GDB logs preserve the UI
evidence. Binary `36bccc326134b42f38a826ee01d60540ca3a7c2337a984222f56a929d3f7261f`
also passed all network, navigation and reload assertions, then returned
zero after native window-close confirmation. The separately recorded
`browser-only-proof.json` identifies its unique Omega WebSocket payload
and nonoverlapping pending TLS connection; earlier raw probes used an
umlaut payload. The original preflight timestamp was written after browser
traffic and is explicitly not used as a traffic cutoff.

The final executable
`74b6ade751d6e79d4b81299dbd6632e8b112a95b0c17325f434a5645b4c0ce94`
passed the complete automated HTTPS/WebSocket/concurrency/cancellation,
navigation/reload and native Quit matrix in
`build/amigaos3-browser-release-final`. This run contained only browser
traffic and used no stream tracing. `release-proof.json` checks the actual
server exchange, pending TLS bytes/close and `RETURN=0`/`BROWSER_EXIT`.
The only change after the separately input-tested `36bccc` binary was
removing disabled trace code's unnecessary errno accesses; the frontend
input evidence remains explicitly associated with that earlier hash.
Both runs disabled synchronous TopSites screenshots through the recorded
fixture option. This does not establish performance with those thumbnails
enabled, live Internet-stack behavior, physical hardware or Quit while a
connection remains active.

The native fixture helper logs the original UTC/locale/DST settings and sets
only the disposable guest's `timer.device` clock from host UTC. It does not
alter the host clock, battery clock, or source system. Certificate hashes
and the exact guest trust-store file are recorded in `tls-inputs.json`.

The HTTP page loads trusted HTTPS JSON and a script which must actually
execute. Both endpoints explicitly allow the HTTP test origin through
CORS. The page then requests the trusted endpoint by the wrong hostname
(`127.0.0.1`, absent from its DNS-only SAN), and the independent untrusted
endpoint by `localhost`. Both must produce network errors, not timeouts.
The host additionally requires TLS connection evidence and rejects a test
if either negative request reaches HTTP. Thus CORS denial or an unopened
untrusted server cannot masquerade as a successful certificate test.

Add `--navigation` to navigate from the first page to a second page and
then perform a real reload. Both phases must read the per-run localStorage
token set by the first page. This option requires `--data-hdf`; its reports
are recorded separately as `initial`, `navigation`, and `reload` in
`result.json`. It verifies storage across page lifetimes, not across a cold
browser restart or power loss. The default run still performs only the
simple HTTP page checks.
Reports are also printed and appended immediately to `reports.jsonl`, so
results remain available during an interactive hold or a later failure.

Optional `--gdb` takes `screen.png` through the emulator's GDB monitor after
the checks. Inspect that image separately: a successful screenshot command
is not proof of correct rendered content. `--hold SECONDS` keeps the guest
available after assertions for GDB-driven interaction and shutdown testing.
The browser's actual exit code and `BROWSER_EXIT` marker are recorded in
`browser-output.txt`; page success alone does not imply a clean exit. Never
inject keystrokes through the host desktop or a user's terminal.
Once the marker appears during a hold, the runner records the return code
and ends its own emulator process; no separate cleanup command is needed.

The native fixture-only run passed in
`build/amigaos3-browser-fixture-4/fixture-output.txt`: MUI 21.227, 800x600x24
CyberGraphX screen, all eight superclasses available, zero failures. Its GDB
screenshot was inspected and shows the RTG AmigaDOS screen. This establishes
the launch environment, not an Odyssey browser pass.

The host-populated native DOS7 fixture also passed in
`build/amigaos3-browser-fixture-ffs-host/fixture-output.txt`, including the
font readback and disk operations above. The earlier guest-copy variant
passed in `build/amigaos3-browser-fixture-ffs-2`; it was replaced only to
speed up staging. Neither fixture-only result claims browser navigation.

`build/amigaos3-browser-fixture-https/fixture-output.txt` records a passing
native HTTPS fixture setup, including guest clock conversion. OpenSSL
verified the generated localhost chain and rejected the wrong hostname
and independent untrusted certificate. Generated page scripts pass Node's
syntax check. These are fixture checks; actual Odyssey HTTPS and navigation
results require the optional browser run described above.
