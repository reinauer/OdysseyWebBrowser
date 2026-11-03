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
import re
import shutil
import subprocess
import sys
import tarfile

import amigaos3_cpu

ROOT = Path(__file__).resolve().parents[2]
VERSION = '62.2'
ARCHIVE = 'icu4c-62_2-src.tgz'
URL = 'https://github.com/unicode-org/icu/releases/download/release-62-2/' + ARCHIVE
SHA256 = 'f51976e6893df7b08e594fa798196f80cdf07df885fec129f55f7afe273b6901'
PATCH = ROOT / 'Tools/AmigaOS3/patches/icu-62.2-m68k.patch'

# The data is linked into the executable, so only what the browser can use is
# kept. Locale data, collation and display names stay for these languages and
# all their regional variants; other locales fall back to root data.
KEEP_LANGUAGES = frozenset(('cs', 'da', 'de', 'el', 'en', 'es', 'fi', 'fr', 'hu', 'it', 'ja',
                            'ko', 'nb', 'nl', 'no', 'pl', 'pt', 'ru', 'sv', 'tr', 'zh'))
# WebKit decodes CJK and the other single-byte encodings itself. These are the
# converters TextCodecICU.cpp opens, resolved through ICU's alias table.
KEEP_CONVERTERS = frozenset((
    'ibm-912_P100-1995', 'ibm-914_P100-1995', 'ibm-915_P100-1995', 'iso-8859_10-1998',
    'ibm-921_P100-1995', 'iso-8859_14-1998', 'ibm-923_P100-1998', 'ibm-878_P100-1996',
    'macos-0_2-10.2', 'ibm-5346_P100-1998', 'ibm-5347_P100-1998', 'ibm-5350_P100-1998',
    'ibm-9448_X100-2005', 'ibm-5354_P100-1998', 'macos-7_3-10.2', 'macos-6_2-10.4',
    'macos-29-10.2', 'macos-35-10.2', 'euc-tw-2014'))
# Trees whose .res items are locales, each with its own res_index.
LOCALE_TREES = ('', 'brkitr', 'coll', 'curr', 'lang', 'rbnf', 'region', 'unit', 'zone')
LOCALE_NAME = re.compile(r'^[a-z]{2,3}(_[A-Za-z0-9]+)*$')


def removed_data_item(name):
    tree, _, leaf = name.rpartition('/')
    stem, extension = os.path.splitext(leaf)
    if tree == 'translit':
        return True  # no browser or JavaScript API uses transliteration
    if name in ('brkitr/cjdict.dict', 'unames.icu'):
        return True  # Chinese/Japanese word dictionary; character names
    if extension == '.cnv':
        return stem not in KEEP_CONVERTERS
    if extension == '.res' and tree in LOCALE_TREES and LOCALE_NAME.match(stem):
        return stem.split('_')[0] not in KEEP_LANGUAGES
    return False


def rebuild_bundle(host, package, extracted, added, tree, bundle, transform, work, logs, env):
    """Replace one resource bundle with a filtered copy built by genrb."""
    name = (tree + '/' if tree else '') + bundle + '.res'
    run([host / 'bin/icupkg', '-d', extracted, '-x', name, package],
        work, logs / 'data-bundle-extract.log', env)
    directory = extracted / tree if tree else extracted
    dump = subprocess.check_output([str(host / 'bin/derb'), '-s', str(directory), '-c', bundle],
                                   env=env, text=True)
    output = added / tree if tree else added
    output.mkdir(parents=True, exist_ok=True)
    # derb's header comment names a "(null)" coding, which genrb would read.
    text = transform('\n'.join(line for line in dump.splitlines() if not line.startswith('//')) + '\n')
    (output / (bundle + '.txt')).write_text(text)
    run([host / 'bin/genrb', '-s', output, '-d', output, bundle + '.txt'],
        work, logs / 'data-bundle-genrb.log', env)
    (output / (bundle + '.txt')).unlink()
    return name


def locale_index(name):
    def transform(dump):
        version = re.search(r'CLDRVersion \{ "([^"]+)" \}', dump)
        locales = re.findall(r'^\s+([A-Za-z0-9_]+) \{ "" \}$', dump, re.M)
        if not locales:
            raise RuntimeError('No installed locales in ' + name)
        kept = [locale for locale in locales if locale.split('_')[0] in KEEP_LANGUAGES]
        text = 'res_index:table(nofallback) {\n'
        if version:
            text += '    CLDRVersion { "%s" }\n' % version.group(1)
        return text + '    InstalledLocales {\n' + ''.join('        %s {""}\n' % locale for locale in kept) + '    }\n}\n'
    return transform


def without_cjk_dictionary(dump):
    """Drop the Han, Hiragana and Katakana word dictionary from break iteration.

    Without it ICU treats a run of these scripts as one word; line breaking
    does not use the dictionary."""
    lines = dump.splitlines()
    kept = [line for line in lines if 'cjdict.dict' not in line]
    if len(lines) - len(kept) != 6:
        raise RuntimeError('Unexpected cjdict references in brkitr/root')
    return '\n'.join(kept) + '\n'


def filter_data(host, source, work, logs, env):
    """Write the item lists for icupkg: removals and rebuilt bundles."""
    filtered = work / 'data-filter'
    if filtered.exists():
        shutil.rmtree(filtered)
    extracted, added = filtered / 'original', filtered / 'added'
    for path in (extracted, added):
        path.mkdir(parents=True)
    package = source / 'data/in/icudt62l.dat'
    items = subprocess.check_output([str(host / 'bin/icupkg'), '-l', str(package)],
                                    env=env, text=True).split()
    removed = set(name for name in items if removed_data_item(name))
    rebuilt = []
    for tree in LOCALE_TREES:
        if (tree + '/' if tree else '') + 'res_index.res' in items:
            rebuilt.append(rebuild_bundle(host, package, extracted, added, tree, 'res_index',
                                          locale_index(tree), work, logs, env))
    rebuilt.append(rebuild_bundle(host, package, extracted, added, 'brkitr', 'root',
                                  without_cjk_dictionary, work, logs, env))
    (filtered / 'add.txt').write_text('\n'.join(rebuilt) + '\n')
    # Kept items may alias removed locales (no_NO_NY to nn_NO, for example).
    # icupkg refuses a package with missing dependencies; restore those items.
    restored = []
    probe = filtered / 'probe.dat'
    while True:
        (filtered / 'remove.txt').write_text('\n'.join(sorted(removed)) + '\n')
        result = subprocess.run([str(host / 'bin/icupkg'), '-r', str(filtered / 'remove.txt'),
                                 '-s', str(added), '-a', str(filtered / 'add.txt'), str(package), str(probe)],
                                env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        missing = set(re.findall(r'depends on missing item (\S+)', result.stdout))
        if not result.returncode and not missing:
            break
        if not missing or not missing <= removed:
            raise RuntimeError('icupkg rejected the filtered data:\n' + result.stdout)
        removed -= missing
        restored += sorted(missing)
    probe.unlink()
    return filtered, sorted(removed), rebuilt, restored


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
    parser.add_argument('--full-data', action='store_true',
                        help='link the complete ICU data instead of the browser subset')
    amigaos3_cpu.add_argument(parser)
    args = parser.parse_args()
    cpu = amigaos3_cpu.flag_string(args.cpu)
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
    env.update(CFLAGS=cpu + ' -O2', CXXFLAGS=cpu + ' -O2 -std=c++11',
               CPPFLAGS='-DU_HAVE_MMAP=0 -DU_HAVE_POPEN=0 -D_GNU_SOURCE',
               icu_cv_host_frag='mh-linux', ac_cv_c_bigendian='yes')
    if runtime:
        env['CXXFLAGS'] += ' ' + ' '.join(runtime_flags)
        env['LDFLAGS'] = '-L' + str(runtime / 'lib')
    configuration = {'compiler': sha(Path(env['CXX'])), 'full_data': args.full_data,
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

    # Swap the upstream package with the native host tool, removing the items
    # the browser cannot use. genccode C output is target-neutral, unlike its
    # host-selected object/assembly modes.
    data = target / 'icudt62b.dat'
    data_filter = []
    if not args.full_data:
        filtered, removed, rebuilt, restored = filter_data(host, source, work, logs, host_env)
        data_filter = ['-r', filtered / 'remove.txt', '-s', filtered / 'added', '-a', filtered / 'add.txt']
    run([host / 'bin/icupkg', '-tb', *data_filter, source / 'data/in/icudt62l.dat', data],
        target, logs / 'data-swap.log', host_env)
    header = data.read_bytes()[:24]
    if header[2:4] != b'\xda\x27' or header[8:12] != bytes([1, 0, 2, 0]):
        raise RuntimeError('Expected big-endian ASCII ICU data with 16-bit UChar')
    run([host / 'bin/genccode', data.name], target, logs / 'data-code.log', host_env)
    # A constant array goes into the code hunk, which then needs one larger
    # contiguous block to load. Without const it gets a data hunk of its own.
    generated = target / 'icudt62b_dat.c'
    code = generated.read_text()
    if code.count('\nconst struct {\n') != 1:
        raise RuntimeError('Unexpected genccode output layout')
    generated.write_text(code.replace('\nconst struct {\n', '\nstruct {\n'))
    run([env['CC'], *cpu.split(), '-O2', '-Dicudt62b_dat=icudt62_dat',
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
    run([env['CC'], *cpu.split(), '-O2', '-I' + str(ROOT / 'Source/WTF'), '-c',
         ROOT / 'Source/WTF/wtf/amigaos3/Time.c', '-o', 'amiga-time.o'],
        target, logs / 'probe-time.log', env)
    run([env['CC'], *cpu.split(), '-O2', '-c',
         ROOT / 'Source/WTF/wtf/amigaos3/Malloc.c', '-o', 'amiga-malloc.o'],
        target, logs / 'probe-malloc.log', env)
    run([env['CC'], *cpu.split(), '-O2', '-c',
         ROOT / 'Source/WTF/wtf/amigaos3/TTY.c', '-o', 'amiga-tty.o'],
        target, logs / 'probe-tty.log', env)
    math_helpers = amigaos3_cpu.math_helpers(args.cpu)
    if runtime and (runtime / 'lib/integer-math.o').exists():
        math_helpers += ('integer-math.o',)
    runtime_libraries = ([str(runtime / 'lib' / name) for name in math_helpers]
        + ['-L' + str(runtime / 'lib')]) if runtime else []
    run([env['CXX'], *cpu.split(), '-O2', '-std=c++11'] + runtime_flags +
        ['-I' + str(prefix / 'include'),
         ROOT / 'Tools/AmigaOS3/icu-test.cpp', 'amiga-time.o', 'amiga-malloc.o', 'amiga-tty.o'] +
        runtime_libraries + ['-L' + str(prefix / 'lib'), '-licui18n', '-licuuc', '-licudata',
         '-lstdc++', '-lpthread', '-latomic',
         '-Wl,--wrap=_malloc_r,--wrap=_free_r,--wrap=_calloc_r,--wrap=_realloc_r,--wrap=isatty',
         '-o', 'icu-test'], target, logs / 'probe-link.log', env)
    manifest = {'version': VERSION, 'source_url': URL, 'source_sha256': SHA256,
                'patch_sha256': patch_hash, 'cpu': args.cpu, 'cpu_flags': cpu.split(),
                'data_endianness': 'big', 'data_section': 'data',
                'data_filter': None if args.full_data else {
                    'languages': sorted(KEEP_LANGUAGES), 'converters': sorted(KEEP_CONVERTERS),
                    'removed_items': len(removed), 'rebuilt_bundles': rebuilt,
                    'restored_dependencies': restored},
                'data_bytes': data.stat().st_size,
                'data_sha256': sha(data), 'probe': str(target / 'icu-test'),
                'cxx_runtime': str(runtime) if runtime else None,
                'time_source_sha256': sha(ROOT / 'Source/WTF/wtf/amigaos3/Time.c'),
                'malloc_source_sha256': sha(ROOT / 'Source/WTF/wtf/amigaos3/Malloc.c'),
                'tty_source_sha256': sha(ROOT / 'Source/WTF/wtf/amigaos3/TTY.c'),
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
