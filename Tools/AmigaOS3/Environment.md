# Native startup environment

The installed newlib `getenv` searches POSIX `environ`; its startup does not
populate that array from AmigaDOS environment variables. Thus a Shell
`SetEnv` or `Set` can be visible to DOS `GetVar` while C/C++ `getenv` returns
null. In particular, static initializers reading browser configuration
cannot be fixed by importing variables later in `main`.

`Source/WTF/wtf/amigaos3/Environment.c` takes a startup snapshot using
`ADD2INIT(..., 0)`. The SDK opens libraries at priority -100, initializes
pthread support at -6 when linked, and invokes C++ constructors at +100.
The native regression verifies this ordering with a real C++ constructor.
The SDK heap is initially zero-filled; its constructors do not reset live
allocations. The port's recursive heap lock initializes lazily. The importer
uses DOS I/O without stdio and preserves `errno` and DOS `IoErr`.

The importer first examines regular files directly in `ENV:`, rejecting
binary contents (NUL and non-text control bytes). It does not traverse
preference directories. It then visits the actual process `pr_LocalVars`
list, accepting only `LV_VAR` entries without `LVF_IGNORE` or
`GVF_BINARY_VAR`; aliases and binary locals are excluded. `GetVar` supplies
the native single-line text value. Local values replace global values with
the same spelling. There is no browser-variable allowlist.

Names retain their original spelling, and subsequent POSIX `getenv` is
case-sensitive; this does not emulate DOS's case-insensitive lookup. Use
consistent spelling, such as `OWB_DISABLE_TOPSITES_SCREENSHOTS`, in the
Shell and the application. Arbitrary nested `ENV:` paths and binary values
are not imported. This snapshot does not monitor later DOS `SetVar` calls.
Normal libc `setenv` and `unsetenv` remain available after initialization.

`setenv` copies accepted names and values into libc-owned storage. No
pointer into a DOS local-variable record or temporary input buffer escapes.
The source is an explicit member of CMake's `AmigaOS3Allocator` object
target, so startup registration is retained even when nothing references
the function from application code.

## Native regression

The standalone target uses the same production importer and allocator
wrapper as the browser:

```sh
cmake -S Tools/AmigaOS3 -B build/amigaos3-environment-cmake -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/Source/cmake/AmigaOS3.cmake" \
  -DAMIGAOS3_TOOLCHAIN_ROOT=/opt/amiga-16.2-20261029 \
  -DAMIGAOS3_RUNTIME_ROOT="$PWD/build/amigaos3-sdk/cxx-runtime-oct29"
cmake --build build/amigaos3-environment-cmake \
  --target odyssey-environment-test -j4
python3 Tools/AmigaOS3/run-platform-test.py \
  --system build/amigaos3-amissl-fixture \
  --rom "$HOME/AmigaOS/AmigaOS-3.2-full/ROM/kicka1200.rom" \
  --emulator "$HOME/git/WinUAE_mac/WinUAE/bin/winuae" \
  --binary build/amigaos3-environment-cmake/odyssey-environment-test \
  --output build/amigaos3-environment-cmake/native \
  --gdb-port 2351 --success-marker 'Native environment: 0 failures'
```

Use a fresh output directory and an unused emulator port. The regression
creates its named global/local variables only in the disposable guest.
It checks constructor-time global and local access, local precedence,
spaces, tab, UTF-8, an empty value, a 4096-byte value, aliases, ignored
locals, binary global files, and a binary local whose contents are all
printable. The latter reports the actual native `lv_Flags` value `0400`.
Heap churn after import verifies copied value lifetime, and ordinary
`setenv`/`unsetenv` are exercised afterward.

The direct native compile of the final production source passes in
`build/amigaos3-environment/native-2/output.txt`: setup, C++ constructor,
value lifetime and total failures are all zero, with `RETURN=0`.
`build/amigaos3-environment/manifest.json` records the exact commands,
compiler/source/object/executable hashes and runner identity. This evidence
proves the startup import independently; final browser flag behavior is
validated by the browser's own native run.

The exact CMake integration target also passes in
`build/amigaos3-environment/native-cmake/output.txt` with the same zero
failure counts and `RETURN=0`. The binary was built in
`build/amigaos3-platform-oct29-env/odyssey-environment-test`; the runner's
`inputs.json` pins its hash and all copied guest inputs. This additionally
verifies that the shared object target retains the init-list registration.
