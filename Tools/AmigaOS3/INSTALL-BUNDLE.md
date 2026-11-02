# Local installation bundle

`stage-install.py` copies explicit inputs into a new `Odyssey` directory.
It leaves Dist, SDK prefixes and runtime inputs unchanged and rejects an
existing output. It includes the native HUNK executable, distribution
assets, the tested relocatable Fonts/Conf files, notices and provenance.
No system installation or external runtime files are collected.

Fetch the newlib notice from the exact inspected revision, once:

```sh
curl -fL https://raw.githubusercontent.com/AmigaPorts/newlib-cygwin/fb0a76ceacae0c7c1442d5a1e00be6ba7f669de2/COPYING.NEWLIB \
  -o build/amigaos3-browser-support/COPYING.NEWLIB
```

After the final executable is built and tested, stage it:

```sh
python3 Tools/AmigaOS3/stage-install.py \
  --binary build/amigaos3-browser-oct29/bin/OWB \
  --source-root "$PWD" \
  --dependency-root build/amigaos3-sdk-oct29 \
  --runtime-root build/amigaos3-sdk/cxx-runtime-oct29 \
  --pthread-sources build/amigaos3-gthreads-oct29/pthread-source/pthreads \
  --newlib-license build/amigaos3-browser-support/COPYING.NEWLIB \
  --output build/amigaos3-install-final \
  --archive build/Odyssey-AmigaOS3-local.zip
```

`--lha-archive PATH` also packs an LHA archive for use on the Amiga, which
needs `lha` on the host's PATH. The build directory's `lha` target stages the
current `OWB` and writes `Odyssey-AmigaOS3.lha`:

```sh
ninja -C build/amigaos3-browser-oct29 lha
```

The cache variables `AMIGAOS3_NEWLIB_LICENSE` and `AMIGAOS3_PTHREAD_SOURCES`
select the notice inputs. The target's staging directory is `lha-stage`.

Packing uses `lha cqgo5`. Native LhA 2.15 refused `-lh7-` entries with RC=10,
so `-lh5-` is used, which also suits slow CPUs. Generic headers leave out the
builder's UID. `Resource/icudt49b` is left out of both archive types, and also
out of the staged directory, because the port links its ICU data. The LHA
alone also contains `Odyssey.info`, the drawer icon, beside the directory.
Native LhA 2.15 unpacked it to a fresh DOS7 volume in 5 minutes 44 seconds on a
cycle-exact 50 MHz 68030, and all files matched.

Both archives use sorted entries and fixed timestamps; the same inputs
produce the same archive, and a `.sha256` file beside each records its checksum.
The inputs include the source commit and a hash of tracked changes, recorded in
`Build/inputs.json`, so any commit or tracked edit changes the archive's hash.
Inside the bundle, `SHA256SUMS` covers all files except itself; the input
manifest records source roots and hashes, including linked library inputs.
This identifies the supplied files; it does not infer that the supplied
binary was linked from them. Match its hash to the final native test record.

The dependency builder stages complete library notices, including the
case-sensitive libxml2/libxslt `Copyright` files, FreeType's selected-license
texts and SQLite's public-domain notice. The runtime's GCC exception and
license texts, pinned newlib notice, and verbatim pthread source notices
are included. Proprietary OS/MUI/AmiSSL runtime files are not collected.
See the generated `AmigaOS3.readme` for installation and tested requirements.
