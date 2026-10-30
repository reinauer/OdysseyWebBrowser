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

The optional ZIP uses sorted entries and fixed timestamps; the same inputs
produce the same archive. The adjacent `.sha256` file records its checksum.
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
