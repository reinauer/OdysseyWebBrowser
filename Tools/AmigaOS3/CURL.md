# Native curl and AmiSSL

`build-curl.py` builds curl 8.23.0 for a 68020 with an explicitly selected
SDK profile. It downloads the official release archive and verifies
SHA256 `8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5`.
The matched October 29 sources, logs and build products remain in
`build/amigaos3-curl-oct29`; installation updates only
`build/amigaos3-sdk-oct29`. The upstream COPYING file is installed with the
library. See [SDK-PROFILES.md](SDK-PROFILES.md); the older default prefix and
runtime must not be mixed with the moving `/opt/amiga-16.2` symlink.

First build the pthread C++ runtime, ICU, zlib, libpsl and nghttp2 with the
adjacent builders. Then run:

```sh
python3 Tools/AmigaOS3/build-curl.py \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-root build/amigaos3-curl-oct29 \
  --prefix build/amigaos3-sdk-oct29 \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-oct29 --smoke
```

This installs real static curl archives, headers and pkg-config metadata. The
TLS backend uses the compiler SDK's AmiSSL import stubs (AmiSSL 5.27,
OpenSSL 3.6.2), without fake `libssl` or `libcrypto` archives. Public-suffix
checks use libpsl and ICU. DNS resolution is synchronous; no resolver thread
can accidentally use a socket base owned by another Amiga task. IPv6 is
disabled. HTTP/2 uses nghttp2 1.70.0 (`build-dependencies.py nghttp2`, the
library only); `CurlContext` then negotiates it over TLS with ALPN and waits
for an existing connection to a host before opening another, so one
connection per host carries all of a page's requests. On a live page of 19
Wikimedia images, 18 of them from one host, this needed two TLS handshakes
instead of five or six and loaded the page in 21.0 s instead of 29 to 30 s
on a 68040; see
[OPTIMIZATIONS.md](OPTIMIZATIONS.md). The builder records commands, source
hash, installed library hashes and smoke binary hash in `manifest.json`.

SDK adaptations are explicit: `unistd.h` supplies `ssize_t` before the
native socket headers, and curl's two TLS `close` function-pointer calls are
parenthesized so the SDK pthread cancellation macro cannot expand them.
The task-owned native socket error adaptation is described below.

## Task ownership

`AmigaOS3AmiSSL.c` is used by both the browser and native smoke. The browser's
`CurlGlobal` initializes AmiSSL before any curl context members. The owner
opens the master library and its own socket base with `OpenAmiSSLTagList`.
The single curl worker opens its own socket base, binds a private socket
error integer, calls
`InitAmiSSLA`, and calls `CleanupAmiSSLA` before closing that socket base.
Joining the worker restores the owner's socket base. On OS3, stopping the
scheduler is permanent. It skips cancelled or stopped setup tasks, drains
retained finalization callbacks, cancels active transfers and destroys
streams before destroying its multi handle. An
intrusive worker-owned CurlHandle list also closes easy handles belonging to
completed requests awaiting main-thread response callbacks. Those wrappers
are detached and set to null before worker AmiSSL/socket cleanup; later main
task destruction cannot call SSL through the closed worker context. Late
requests are canceled, and late worker jobs cannot restart the scheduler.
An internal curl multi failure aborts before unsafe task-context teardown;
ordinary HTTP/TLS errors still follow the request completion path.

CurlContext is NeverDestroyed, so its destructor cannot supply process
cleanup. The executable explicitly shuts it down after application
destruction and before closing OS libraries, both normally and through
atexit. This joins the worker, releases the share handle, calls
curl_global_cleanup and finally closes owner AmiSSL. Cleanup is idempotent
and does not construct an unused CurlContext. CertificateInfo retains DER
byte vectors, not live X509 pointers.

OS3 stream connections use the shared curl multi handle, so TCP, proxy and
TLS connection setup no longer calls blocking `curl_easy_perform` on the
sole HTTP worker. `CURLMSG_DONE` enables stream I/O and queues `didOpen` on
the main task. The easy handle remains attached until cancellation, EOF,
failure or shutdown, as required by libcurl's
[connect-only multi interface](https://curl.se/libcurl/c/CURLOPT_CONNECT_ONLY.html).
The worker services nonblocking send/receive once per tick, including TLS
buffered input. Pending creation cancelled before execution performs no
connection setup, and shutdown does not start queued connections.

The configured curl resolver is synchronous; name lookup can still block
the worker inside libcurl, just as for HTTP requests. Numeric-host TCP/TLS
stall and cancellation tests isolate the stream fix from that separate DNS
limitation. Full-browser stream, cancellation and normal shutdown checks
passed in `build/amigaos3-browser-network-final`, including strict HTTP
progress while TLS remained pending. The standalone transport probe below
does not exercise the browser scheduler.

The SDK `libamisslauto` constructor does not execute in this newlib startup.
It is therefore excluded from staged metadata and browser imports. Explicit
AmiSSL lifetime also avoids relying on static constructor order. AmiSSL's
[developer documentation](https://github.com/jens-maus/amissl/tree/5.27/dist)
describes the owner/subprocess initialization protocol used here.

## Native verification

Classic newlib's `errno` is process-global in this SDK. A deterministic
native interleaving reproduced a socket returning `EWOULDBLOCK` (35), then
the main task storing `ENOENT` (2), and curl misclassifying the idle receive
as `CURLE_RECV_ERROR` (56). The reproduction is preserved under
`build/amigaos3-connect-only/errno-interleave-native`.

The owner and worker now bind separate error integers to both
`bsdsocket.library` and AmiSSL. The reproducible curl source adaptation
uses native `Errno()` and `SocketBaseTagList(SBTC_ERRNO)` for `SOCKERRNO` and
`SET_SOCKERRNO` on the classic direct-socket backend. It has no Odyssey
symbol dependency and leaves the OS4, MorphOS, AROS and other branches
unchanged. This repairs the network error channel; it does not make generic
SDK libc `errno` thread-local. The previous curl archive and manifest remain
in `build/amigaos3-curl-oct29/socket-error-baseline`.

`build-curl.py --smoke --probes-only` relinks the transport probe against
already staged dependencies without rebuilding or installing curl. Pass the
same explicit SDK, prefix, runtime and build-root arguments as its build.
Native support consumers also require `--wrap=getcwd`, `--wrap=utimes` and
`--wrap=utime`, which both dependency builders propagate to executable links.

The test requires an installed AmigaOS fixture containing AmiSSL under
`AmiSSL/`, with its master library in `Libs/`. Supply your existing ROM and
WinUAE paths; the source fixture is copied, never modified:

```sh
python3 Tools/AmigaOS3/run-curl-test.py \
  --system build/amigaos3-amissl-fixture \
  --rom /path/to/kicka1200.rom \
  --emulator /path/to/winuae \
  --binary build/amigaos3-curl-oct29/curl-test \
  --output build/amigaos3-curl-oct29/network-check
```

The runner serves HTTP on localhost:18787 and HTTPS on localhost:18443 and
uses an isolated GDB port (default 2346). It creates a two-day localhost test
certificate. Every TLS request enables peer and hostname verification. The
native test checks HTTP and verified HTTPS, rejects an untrusted certificate,
rejects a hostname mismatch, and repeats the worker initialization/cleanup
cycle twice. Owner HTTP requests after each join verify socket restoration.
It also tests real descriptor truncation, zero extension and `isatty`.

The minimal fixture originally reported UTC twelve hours behind the host:
WinUAE's host local clock and the fixture's `loc_GMTOffset=-300` disagreed.
The test logs its original UTC and locale offset, then synchronizes only the
disposable guest clock to a host UTC timestamp, converted to native local
seconds since 1978. Production time conversion and TLS verification are
unchanged. No backdated or verification-disabled certificate is used.

Evidence is saved in `guest/output.txt`, `guest/inputs.json` and the runner's
`manifest.json` (probe and certificate hashes, timestamp and host requests).
The repaired matched profile passed all 21 checks with `curl native: 0
failures` and guest return code zero in
`build/amigaos3-curl-oct29/native-socket-error-final`. Its manifest pins curl
archive `df533d56d6de1f0e845fbe79c920753932df736bd34770bee2f5b33fd27abf80`.
This establishes native loopback transport and repeated
AmiSSL task lifetime, not a completed browser or live Internet-stack test.

## File descriptor limits

`dependencies/curl-files.c` uses the selected SDK's checked descriptor accessor
and native DOS APIs. Extension explicitly writes zero bytes, because
`SetFileSize` alone does not initialize new space. Shrinking below the current
file offset returns `ENOTSUP` before mutation: AmigaDOS cannot preserve that
position beyond EOF. Atomic POSIX replacement by `rename` remains a separate
filesystem limitation; this implementation never deletes a destination first.

The final repaired browser passed Unicode WebSocket echo, HTTP while TLS
was still pending, cancellation, subsequent HTTP, HTTPS trust checks,
navigation/reload storage and normal Quit in `browser-release-final`
(binary `74b6ade751d6e79d4b81299dbd6632e8b112a95b0c17325f434a5645b4c0ce94`).
This run used no stream tracing and no independent probe traffic. Trusted
native page input passed separately in `browser-network-final`
(`36bccc326134b42f38a826ee01d60540ca3a7c2337a984222f56a929d3f7261f`);
the subsequent change only removes errno accesses from disabled tracing.
Both runs disabled synchronous TopSites screenshots. The earlier run's
`browser-only-proof.json` separates its unique Unicode exchange from
independent probe traffic.
See
[BROWSER-FIXTURE.md](BROWSER-FIXTURE.md) for exact binary identities and
interaction evidence. Shutdown while a connection is actively pending
remains a separate validation case.
