# Native curl socket error regression

The October 29 SDK exposes the same newlib `errno` address to the owner and
its pthread worker. Binding bsdsocket's error output to that address lets a
main-task libc call overwrite the worker's `EWOULDBLOCK` before curl reads
it. An idle live connection can consequently produce `CURLE_RECV_ERROR`.
The production fix gives each AmiSSL/socket task its own error storage and
makes classic-Amiga curl read/write socket errors through `Errno()` and
`SocketBaseTagList(SBTC_ERRNO)`.

`SocketErrnoTest.c` forces this interleaving in curl's real receive path.
Its private `cf-socket.c` copy pauses **after** `sread()` returns and
**before** curl classifies the error. The main task assigns its own
`errno = ENOENT`, then releases the worker. This is scheduling
instrumentation, not an unmodified full-browser test.

The builder produces two executables without changing installed archives:

* `socket-errno-baseline` restores the former shared error binding and
  `SOCKERRNO` macro only in the private probe. Success means reproducing
  shared errno 35 becoming 2 and curl returning 56 (`CURLE_RECV_ERROR`),
  although the native socket `Errno()` still reports 35.
* `socket-errno-fixed` retains the production task-owned binding and native
  accessor. Success requires the same main-task errno mutation, socket
  error remaining 35, and curl returning 81 (`CURLE_AGAIN`).

Build after the matching runtime, dependencies and repaired curl build:

```sh
python3 Tools/AmigaOS3/build-socket-errno-test.py \
  --toolchain /opt/amiga-16.2-20261029 \
  --prefix build/amigaos3-sdk-oct29 \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-oct29 \
  --curl-build build/amigaos3-curl-oct29 \
  --output build/amigaos3-socket-errno-repro
```

Use a fresh output directory. The builder checks the pinned curl 8.23.0
download, installed archive and build manifest, then records commands and
input/output hashes in `identity.json`. It neither downloads sources nor
starts a server or emulator.

Both executables need an **existing plain HTTP listener** that keeps a new
connection open while waiting for its request. The probe establishes TCP
but sends no HTTP request. The default is `http://127.0.0.1:18788/`; supply
a different HTTP URL as the executable's first argument. It also accepts
a filename containing that URL on its first line, so the existing native
runner's `--script path/to/loopback-url.txt` option can provide it. Enable
`--bsdsocket`, use at least a 262144-byte stack, and run the executables
sequentially on the chosen guest GDB port. Success markers are
`Socket errno baseline: 0 failures` and `Socket errno fixed: 0 failures`;
both must also finish with `RETURN=0`.

Always supply `--script` when using that runner: without it the runner
passes its platform-test-specific `--mui` argument, which this probe rejects.

The original native reproduction and repaired counterpart are retained in
`build/amigaos3-connect-only/errno-interleave-native` and
`errno-interleave-fixed-native`, with source/build identities in
`errno-proof-identity.json` and `errno-fixed-proof-identity.json`.
The tracked probe also passed both expected outcomes with `RETURN=0` in
`build/amigaos3-socket-errno-repro-final/native-baseline/output.txt` and
`native-fixed/output.txt`. The final pinned-source builder produced
byte-identical executables in `build/amigaos3-socket-errno-pinned-recipe`;
its `identity.json` records the exact sources and dependencies.
The repaired library also passed the separate production WebSocket frame
echo while a TLS handshake remained pending. These isolate the error
channel and transport; full-browser results are recorded separately.
