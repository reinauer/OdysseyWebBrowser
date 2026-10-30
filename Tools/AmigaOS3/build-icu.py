#!/usr/bin/env python3
"""Build ICU 62.2 host tools and full static m68k libraries in the checkout.

ICU 62.2 matches Odyssey's bundled headers and uses pthread synchronization,
which the Amiga GCC SDK supports. Its source and Unicode data remain licensed
under the upstream ICU/Unicode terms, copied into the staged prefix.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[2]
VERSION = '62.2'
ARCHIVE = 'icu4c-62_2-src.tgz'
URL = 'https://github.com/unicode-org/icu/releases/download/release-62-2/' + ARCHIVE
SHA256 = 'f51976e6893df7b08e594fa798196f80cdf07df885fec129f55f7afe273b6901'
PATCH = ROOT / 'Tools/AmigaOS3/patches/icu-62.2-m68k.patch'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args, cwd, log, env=None):
    print('Running:', ' '.join(map(str, args)), flush=True)
    with log.open('w') as output:
        result = subprocess.run(list(map(str, args)), cwd=cwd, env=env,
                                stdout=output, stderr=subprocess.STDOUT)
    if result.returncode:
        print('\n'.join(log.read_text(errors='replace').splitlines()[-40:]))
        raise RuntimeError('Command failed; full log: ' + str(log))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path, default=Path('/opt/amiga-16.2'))
    parser.add_argument('--build-root', type=Path, default=ROOT / 'build/amigaos3-deps-icu')
    parser.add_argument('--prefix', type=Path, default=ROOT / 'build/amigaos3-sdk')
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--cxx-runtime', type=Path,
                        help='pthread C++ runtime prefix from build-cxx-runtime.py')
    args = parser.parse_args()
    build, prefix = args.build_root.resolve(), args.prefix.resolve()
    for path in (build, prefix):
        if ROOT not in path.parents:
            parser.error('build root and prefix must stay inside the checkout')
    if args.jobs < 1:
        parser.error('jobs must be positive')
    downloads, work = build / 'downloads', build / ('icu-' + VERSION)
    runtime = args.cxx_runtime.resolve() if args.cxx_runtime else None
    host, source = work / 'host', work / 'icu/source'
    target = work / ('target-pthread' if runtime else 'target')
    runtime_flags = []
    if runtime:
        for required in ('include/c++/m68k-amigaos/bits/c++config.h',
                         'lib/libstdc++.a', 'lib/libgcc.a'):
            if not (runtime / required).is_file():
                parser.error('missing C++ runtime input: ' + str(runtime / required))
        runtime_flags = ['-isystem', str(runtime / 'include/c++/m68k-amigaos'),
                         '-isystem', str(runtime / 'include/c++')]
    logs = work / 'logs'
    for path in (downloads, host, target, logs, prefix / 'lib', prefix / 'include'):
        path.mkdir(parents=True, exist_ok=True)
    archive = downloads / ARCHIVE
    if not archive.exists():
        partial = archive.with_suffix('.part')
        run(['curl', '-fL', '--retry', '3', '-o', partial, URL], build, logs / 'download.log')
        if sha(partial) != SHA256:
            raise RuntimeError('ICU download checksum mismatch')
        partial.rename(archive)
    if sha(archive) != SHA256:
        raise RuntimeError('ICU archive checksum mismatch')
    patch_hash = sha(PATCH)
    stamp = work / 'source-patch.sha256'
    if not source.exists():
        with tarfile.open(archive) as package:
            package.extractall(work, filter='data')
        run(['patch', '-p1', '-i', PATCH], source.parent, logs / 'patch.log')
        stamp.write_text(patch_hash + '\n')
    elif not stamp.exists() or stamp.read_text().strip() != patch_hash:
        raise RuntimeError('Source patch changed; select a fresh --build-root')

    # Never let target CC/flags inherited from a shell configure the host tools.
    host_env = dict(os.environ)
    for name in ('CC', 'CXX', 'AR', 'RANLIB', 'CPPFLAGS', 'CFLAGS', 'CXXFLAGS',
                 'LDFLAGS', 'LIBS', 'PKG_CONFIG_PATH', 'PKG_CONFIG_LIBDIR'):
        host_env.pop(name, None)
    system = 'MacOSX' if sys.platform == 'darwin' else 'Linux'
    options = ['--disable-shared', '--enable-static', '--disable-tests',
               '--disable-samples', '--disable-extras']
    run([source / 'runConfigureICU', system] + options,
        host, logs / 'host-configure.log', host_env)
    run(['make', '-j' + str(args.jobs)], host, logs / 'host-build.log', host_env)

    toolbin = args.toolchain.resolve() / 'bin'
    env = dict(host_env)
    for variable, tool in [('CC', 'gcc'), ('CXX', 'g++'), ('AR', 'ar'), ('RANLIB', 'ranlib')]:
        env[variable] = str(toolbin / ('m68k-amigaos-' + tool))
    env.update(CFLAGS='-m68020 -O2', CXXFLAGS='-m68020 -O2 -std=c++11',
               CPPFLAGS='-DU_HAVE_MMAP=0 -DU_HAVE_POPEN=0 -D_GNU_SOURCE',
               icu_cv_host_frag='mh-linux', ac_cv_c_bigendian='yes')
    if runtime:
        env['CXXFLAGS'] += ' ' + ' '.join(runtime_flags)
        env['LDFLAGS'] = '-L' + str(runtime / 'lib')
    configuration = {'compiler': sha(Path(env['CXX'])),
                     'cflags': env['CFLAGS'], 'cxxflags': env['CXXFLAGS'],
                     'cppflags': env['CPPFLAGS'], 'prefix': str(prefix)}
    if runtime:
        configuration['runtime_config'] = sha(runtime / 'include/c++/m68k-amigaos/bits/c++config.h')
        configuration['runtime_gthreads'] = sha(runtime / 'include/c++/m68k-amigaos/bits/gthr.h')
    configuration_text = json.dumps(configuration, sort_keys=True)
    configuration_stamp = target / 'build-configuration.json'
    if configuration_stamp.exists() and configuration_stamp.read_text() != configuration_text:
        raise RuntimeError('Target compiler/options changed; select a fresh --build-root')
    configuration_stamp.write_text(configuration_text)
    # mh-linux supplies the generic GCC static-archive rules. Shared linking is
    # disabled, and data is built separately below without ELF/Mach-O assembly.
    run([source / 'configure', '--host=m68k-unknown-amigaos',
         '--with-cross-build=' + str(host), '--prefix=' + str(prefix),
         '--disable-tools', '--disable-icuio', '--with-data-packaging=static'] + options,
        target, logs / 'target-configure.log', env)
    for directory in ('common', 'i18n'):
        run(['make', '-C', directory, 'install-headers'], target,
            logs / (directory + '-headers.log'), env)
    run(['make', '-j' + str(args.jobs)], target, logs / 'target-build.log', env)

    # Swap the complete upstream package with the native host tool. genccode C
    # output is target-neutral, unlike its host-selected object/assembly modes.
    data = target / 'icudt62b.dat'
    run([host / 'bin/icupkg', '-tb', source / 'data/in/icudt62l.dat', data],
        target, logs / 'data-swap.log', host_env)
    header = data.read_bytes()[:24]
    if header[2:4] != b'\xda\x27' or header[8:12] != bytes([1, 0, 2, 0]):
        raise RuntimeError('Expected big-endian ASCII ICU data with 16-bit UChar')
    run([host / 'bin/genccode', data.name], target, logs / 'data-code.log', host_env)
    run([env['CC'], '-m68020', '-O2', '-Dicudt62b_dat=icudt62_dat',
         '-I' + str(source / 'common'), '-c', 'icudt62b_dat.c', '-o', 'icudt62b.o'],
        target, logs / 'data-compile.log', env)
    run([env['AR'], 'rcs', 'lib/libicudata.a', 'icudt62b.o'],
        target, logs / 'data-archive.log', env)
    libraries = ['libicuuc.a', 'libicui18n.a', 'libicudata.a']
    for name in libraries:
        shutil.copy2(target / 'lib' / name, prefix / 'lib' / name)
    license_dir = prefix / 'share/licenses/icu'
    license_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source.parent / 'LICENSE', license_dir / 'LICENSE')
    pkgconfig = prefix / 'lib/pkgconfig'
    pkgconfig.mkdir(parents=True, exist_ok=True)
    for component, libs in [('uc', '-licuuc -licudata'),
                            ('i18n', '-licui18n -licuuc -licudata')]:
        (pkgconfig / ('icu-' + component + '.pc')).write_text(
            f'prefix={prefix}\nlibdir=${{prefix}}/lib\nincludedir=${{prefix}}/include\n'
            f'Name: ICU {component}\nDescription: ICU for AmigaOS 3\nVersion: {VERSION}\n'
            f'Libs: -L${{libdir}} {libs}\nLibs.private: -lpthread -latomic\n'
            'Cflags: -I${includedir}\n')

    # Link actual data consumers; the resulting executable can be run with the
    # native guest runner. A successful link alone is not a runtime test.
    run([env['CC'], '-m68020', '-O2', '-I' + str(ROOT / 'Source/WTF'), '-c',
         ROOT / 'Source/WTF/wtf/amigaos3/Time.c', '-o', 'amiga-time.o'],
        target, logs / 'probe-time.log', env)
    run([env['CC'], '-m68020', '-O2', '-c',
         ROOT / 'Source/WTF/wtf/amigaos3/Malloc.c', '-o', 'amiga-malloc.o'],
        target, logs / 'probe-malloc.log', env)
    math_helpers = ('floatunsidf.o', 'floatunsisf.o', 'truncdfsf2.o', 'double-math.o')
    runtime_libraries = ([str(runtime / 'lib' / name) for name in math_helpers]
        + ['-L' + str(runtime / 'lib')]) if runtime else []
    run([env['CXX'], '-m68020', '-O2', '-std=c++11'] + runtime_flags +
        ['-I' + str(prefix / 'include'),
         ROOT / 'Tools/AmigaOS3/icu-test.cpp', 'amiga-time.o', 'amiga-malloc.o'] +
        runtime_libraries + ['-L' + str(prefix / 'lib'), '-licui18n', '-licuuc', '-licudata',
         '-lstdc++', '-lpthread', '-latomic',
         '-Wl,--wrap=_malloc_r,--wrap=_free_r,--wrap=_calloc_r,--wrap=_realloc_r',
         '-o', 'icu-test'], target, logs / 'probe-link.log', env)
    manifest = {'version': VERSION, 'source_url': URL, 'source_sha256': SHA256,
                'patch_sha256': patch_hash, 'cpu': '68020', 'data_endianness': 'big',
                'data_sha256': sha(data), 'probe': str(target / 'icu-test'),
                'cxx_runtime': str(runtime) if runtime else None,
                'time_source_sha256': sha(ROOT / 'Source/WTF/wtf/amigaos3/Time.c'),
                'malloc_source_sha256': sha(ROOT / 'Source/WTF/wtf/amigaos3/Malloc.c'),
                'compiler': subprocess.check_output([env['CC'], '--version'], text=True).splitlines()[0],
                'libraries': {name: sha(prefix / 'lib' / name) for name in libraries},
                'probe_sha256': sha(target / 'icu-test')}
    if runtime:
        manifest['cxx_runtime_inputs'] = {
            name: sha(runtime / name) for name in
            ('include/c++/m68k-amigaos/bits/c++config.h',
             'include/c++/m68k-amigaos/bits/gthr.h',
             'lib/libstdc++.a', 'lib/libgcc.a',
             *(str(Path('lib') / name) for name in math_helpers))}
    (prefix / 'icu-build.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Staged ICU', VERSION, 'in', prefix)
    print('Native runtime probe (not executed):', target / 'icu-test')


if __name__ == '__main__':
    main()
