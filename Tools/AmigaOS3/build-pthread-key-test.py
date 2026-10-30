#!/usr/bin/env python3
"""Build isolated Oct29 pthread key diagnostics without changing any library."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--toolchain', type=Path, default=Path('/opt/amiga-16.2-20261029'))
    p.add_argument('--repaired-library', type=Path, default=ROOT/'build/amigaos3-sdk/cxx-runtime-oct29/lib/libpthread.a')
    p.add_argument('--output', type=Path, default=ROOT/'build/amigaos3-pthread-key-audit')
    a = p.parse_args()
    cc = a.toolchain/'bin/m68k-amigaos-gcc'
    baseline = a.toolchain/'m68k-amigaos/lib/libm020/libpthread.a'
    libc = a.toolchain/'m68k-amigaos/lib/libm020/libc.a'
    source = ROOT/'Tools/AmigaOS3/PthreadKeyTest.c'
    for path in (cc, baseline, a.repaired_library, libc):
        if not path.is_file(): p.error('missing input: '+str(path))
    output = a.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    manifest = {'source_sha256':digest(source), 'inputs':{str(path.resolve()):digest(path)
                for path in (cc, baseline, a.repaired_library, libc)}, 'executables':{}}
    for profile, library, broken in (('baseline', baseline, 1), ('repaired', a.repaired_library, 0)):
        for mode, name in enumerate(('allocation', 'setspecific', 'delete')):
            binary = output/(profile+'-'+name)
            command = [str(cc.resolve()), '-m68020','-O2','-g', '-DKEY_TEST_MODE='+str(mode),
                       '-DEXPECT_BROKEN='+str(broken), str(source), str(library.resolve()),
                       '-Wl,--wrap=realloc','-lamiga','-o',str(binary)]
            with (output/(binary.name+'.log')).open('w') as log:
                subprocess.run(command, check=True, stdout=log, stderr=log)
            manifest['executables'][binary.name] = {'command':command, 'sha256':digest(binary)}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(output)


if __name__ == '__main__':
    main()
