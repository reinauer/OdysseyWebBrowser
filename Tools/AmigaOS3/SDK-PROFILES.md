# Matched SDK dependency profiles

Keep the original October 14 dependency prefix and runtime intact. The SDK
symlink changed during development, and October 29's newlib changed the
`_reent` layout from 240 to 76 bytes. Several old dependency archives also
referenced the removed `__locale_ctype_ptr` function. Replacing only those
archives would leave an incompletely audited mixture of SDK ABIs.

The complete October 29 rebuild uses these separate paths:

| Input or output | Path |
| --- | --- |
| Compiler and SDK | `/opt/amiga-16.2-20261029` |
| Pthread C++ runtime | `build/amigaos3-sdk/cxx-runtime-oct29` |
| All browser dependencies | `build/amigaos3-sdk-oct29` |
| ICU objects and host tools | `build/amigaos3-icu-oct29` |
| Graphics, fonts, XML, PSL and SQLite objects | `build/amigaos3-deps-oct29` |
| Curl objects and network probe | `build/amigaos3-curl-oct29` |

The SDK content fingerprint is
`047486a65ed6ae2711feedbd12187671e03edafe88d3e1c2963396b4f2f38823`.
The runtime manifest records the complete compiler, libraries, headers and
source revisions. Its split pthread archive replaces only `pthread_key.o`
and `pthread_cond.o`; the remaining SDK members are retained and checked.
See [RUNTIME.md](RUNTIME.md) and [UPSTREAM.md](UPSTREAM.md) for the repairs.

Reproduce the profile from the checkout root:

```sh
python3 Tools/AmigaOS3/build-cxx-runtime.py --profile oct29 \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-dir build/amigaos3-gthreads-oct29 \
  --prefix build/amigaos3-sdk/cxx-runtime-oct29

python3 Tools/AmigaOS3/build-icu.py \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-root build/amigaos3-icu-oct29 \
  --prefix build/amigaos3-sdk-oct29 \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-oct29

python3 Tools/AmigaOS3/build-dependencies.py \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-root build/amigaos3-deps-oct29 \
  --prefix build/amigaos3-sdk-oct29 \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-oct29 \
  --smoke --font-smoke --psl-smoke \
  --sqlite-smoke --sqlite-volume DH1: --file-smoke --file-volume DH1:

python3 Tools/AmigaOS3/build-curl.py \
  --toolchain /opt/amiga-16.2-20261029 \
  --build-root build/amigaos3-curl-oct29 \
  --prefix build/amigaos3-sdk-oct29 \
  --cxx-runtime build/amigaos3-sdk/cxx-runtime-oct29 --smoke
```

Every manual probe links the five explicit compiler helper objects,
`Malloc.c`, and `TTY.c`, with the four allocation wrappers and `--wrap=isatty`.
Executables using native filesystem support additionally wrap `getcwd`,
`utimes` and `utime` to avoid bundled SDK symbol collisions.
The dependency builder's `--probes-only` mode relinks without modifying
dependency archives. Build logs and manifests retain commands and hashes.

Native component results on the final matched profile:

| Probe | Result | Evidence below `build/` |
| --- | --- | --- |
| ICU 62.2 | Six checks, including concurrent locale caches and formatters | `amigaos3-icu-oct29/native/output.txt` |
| Cairo/pixman, PNG, fontconfig, XML/XSLT, directory and metadata support | Passed | `amigaos3-deps-oct29/native-graphics-support/output.txt` |
| FreeType/HarfBuzz/ICU/fontconfig/Cairo | Font discovered; `office` shaped into four glyphs; 559 rendered pixels | `amigaos3-deps-oct29/native-fonts-support/output.txt` |
| Public suffixes | Cookie boundaries, IPv6 addresses and IDN passed | `amigaos3-deps-oct29/native-psl-support/output.txt` |
| Filesystem on real FFS | 59 checks, zero failures, including cwd and DST-independent timestamps | `amigaos3-deps-oct29/native-files-ffs-support/output.txt` |
| SQLite on real FFS | Commit/reopen, cross-process exclusion, rollback and creation guards passed | `amigaos3-deps-oct29/native-sqlite-ffs-support/output.txt` |
| Curl/AmiSSL | 21 checks; HTTP, verified HTTPS, certificate rejection and two worker lifecycles | `amigaos3-curl-oct29/native-support-final/guest/output.txt` |

Every listed guest returned zero. The final probe binaries match the copies
actually executed. `build/amigaos3-sdk-oct29/profile-validation.json` records
the SDK identity and hashes of the evidence; the package manifests record
archive and probe hashes. All public headers shared with the old dependency
prefix are byte-identical. None of the rebuilt archives references the
removed locale function; the full archive audit is saved beside the build.

The full-browser link subsequently exposed another SDK alias dependency:
`mktime` references `div`, which shares an archive member with already
replaced compiler helpers and `ldiv`. The current `integer-math.o` additionally
provides the ordinary C `div` entry point. Its focused native regression
passed 45 signed/unsigned boundary, ABI and `mktime` checks in
`build/amigaos3-integer-alias/native/output.txt`. The preceding runtime
manifest and object remain in that directory; earlier component and JSC
results remain tied to their original executable hashes. The aggregate
manifest records this follow-up separately rather than claiming those
earlier executables were rerun with the changed object.

The focused division probe deliberately links the SDK's real `mktime` and
`ldiv` with the explicit integer helper object:

```sh
mkdir -p build/amigaos3-integer-alias
/opt/amiga-16.2-20261029/bin/m68k-amigaos-gcc -m68020 -O2 \
  Tools/AmigaOS3/integer-div-test.c \
  build/amigaos3-sdk/cxx-runtime-oct29/lib/integer-math.o \
  -Wl,-Map,build/amigaos3-integer-alias/link.map \
  -o build/amigaos3-integer-alias/integer-div-test
```

Run it with `run-platform-test.py` and success marker
`Integer division: 45 checks, 0 failures`.

Use [ICU.md](ICU.md), [CURL.md](CURL.md), and
[dependencies/README.md](dependencies/README.md) for the native runner
commands and fixture requirements. The font fixture needs staged `Conf`
and `Fonts` directories beside the executable. The FFS probes require a
private copy of the test hardfile mounted as `DH1:`. The curl fixture records
and corrects only its disposable guest clock; production certificate and
hostname verification remain enabled.

These are native component results, not proof of a functioning browser UI
or complete browsing session. Full-browser validation must select this
runtime and dependency prefix together and is recorded separately.
