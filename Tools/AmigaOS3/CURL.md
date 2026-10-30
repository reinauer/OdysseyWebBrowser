# Native curl and AmiSSL

`build-curl.py` builds curl 8.23.0 for a 68020 with the SDK at
`/opt/amiga-16.2`. It downloads the official release archive and verifies
SHA256 `8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5`.
Sources, logs and build products remain in `build/amigaos3-deps-curl`;
installation only updates the shared `build/amigaos3-sdk` prefix. The upstream
COPYING file is installed with the library.

First build the pthread C++ runtime, ICU, zlib and libpsl with the adjacent
builders. Then run:

```sh
python3 Tools/AmigaOS3/build-curl.py --smoke
```

This installs real static curl archives, headers and pkg-config metadata. The
TLS backend uses the compiler SDK's AmiSSL import stubs (AmiSSL 5.27,
OpenSSL 3.6.2), without fake `libssl` or `libcrypto` archives. Public-suffix
checks use libpsl and ICU. DNS resolution is synchronous; no resolver thread
can accidentally use a socket base owned by another Amiga task. IPv6 and HTTP/2
are currently disabled. The builder records commands, source hash, installed
library hash and smoke binary hash in `manifest.json`.

Two SDK adaptations are explicit: `unistd.h` supplies `ssize_t` before the
native socket headers, and curl's two TLS `close` function-pointer calls are
parenthesized so the SDK pthread cancellation macro cannot expand them.

## Task ownership

`AmigaOS3AmiSSL.c` is used by both the browser and native smoke. The browser's
`CurlGlobal` initializes AmiSSL before any curl context members. The owner
opens the master library and its own socket base with `OpenAmiSSLTagList`.
The single curl worker opens its own socket base, binds its own `errno`, calls
`InitAmiSSLA`, and calls `CleanupAmiSSLA` before closing that socket base.
Joining the worker restores the owner's socket base. On OS3, stopping the
scheduler is permanent. It drains queued setup/finalization, cancels active
transfers and destroys streams before destroying its multi handle. An
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

The inherited merged-stream scheduler still has its tryToTransfer loop
commented out. Stream connections can be created, but ongoing stream I/O
(WebSockets) is not implemented by this port. That loop was not enabled as
part of the lifetime repair.

The SDK `libamisslauto` constructor does not execute in this newlib startup.
It is therefore excluded from staged metadata and browser imports. Explicit
AmiSSL lifetime also avoids relying on static constructor order. AmiSSL's
[developer documentation](https://github.com/jens-maus/amissl/tree/5.27/dist)
describes the owner/subprocess initialization protocol used here.

## Native verification

The test requires an installed AmigaOS fixture containing AmiSSL under
`AmiSSL/`, with its master library in `Libs/`. Supply your existing ROM and
WinUAE paths; the source fixture is copied, never modified:

```sh
python3 Tools/AmigaOS3/run-curl-test.py \
  --system build/amigaos3-amissl-fixture \
  --rom /path/to/kicka1200.rom \
  --emulator /path/to/winuae \
  --output build/amigaos3-deps-curl/network-check
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
`network-final-3` passed all checks with `curl native: 0 failures` and guest
return code zero. This establishes native loopback transport and repeated
AmiSSL task lifetime, not a completed browser or live Internet-stack test.

## File descriptor limits

`dependencies/curl-files.c` uses the selected SDK's checked descriptor accessor
and native DOS APIs. Extension explicitly writes zero bytes, because
`SetFileSize` alone does not initialize new space. Shrinking below the current
file offset returns `ENOTSUP` before mutation: AmigaDOS cannot preserve that
position beyond EOF. Atomic POSIX replacement by `rename` remains a separate
filesystem limitation; this implementation never deletes a destination first.

The browser shutdown changes were checked by compiling the actual
CurlContext, CurlRequestScheduler and CurlRequest translation units and
reviewing owner/worker teardown order. The native transport probe above
uses the shared AmiSSL lifecycle helper; it does not execute the complete
browser scheduler or deferred-response shutdown. Full-browser shutdown,
late callback delivery and stream behavior still require integration
validation once the browser links.
