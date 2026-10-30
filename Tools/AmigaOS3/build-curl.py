#!/usr/bin/env python3
"""Build pinned native curl/AmiSSL without modifying the compiler SDK.

Requires build-dependencies.py zlib libpsl and build-icu.py first. Runtime
AmiSSL task initialization uses the same explicit owner/worker helper as the
browser; newlib does not run the SDK auto archive's constructor.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
VERSION = '8.23.0'
URL = f'https://curl.se/download/curl-{VERSION}.tar.xz'
SHA256 = '8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path, default=Path('/opt/amiga-16.2'))
    parser.add_argument('--prefix', type=Path, default=ROOT/'build/amigaos3-sdk')
    parser.add_argument('--build-root', type=Path, default=ROOT/'build/amigaos3-deps-curl')
    parser.add_argument('--cxx-runtime', type=Path, help='Native runtime prefix (default: PREFIX/cxx-runtime)')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--smoke', action='store_true')
    parser.add_argument('--probes-only', action='store_true', help='Relink the smoke probe without modifying staged libraries')
    args = parser.parse_args()
    if args.probes_only and not args.smoke:
        parser.error('--probes-only requires --smoke')
    prefix, build, toolchain = (p.resolve() for p in (args.prefix, args.build_root, args.toolchain))
    runtime = (args.cxx_runtime or prefix/'cxx-runtime').resolve()
    math_helpers = ['floatunsidf.o', 'floatunsisf.o', 'truncdfsf2.o', 'double-math.o']
    if (runtime/'lib/integer-math.o').exists():
        math_helpers.append('integer-math.o')
    for name in ('downloads', 'logs', 'target'):
        (build/name).mkdir(parents=True, exist_ok=True)
    for required in (prefix/'lib/libz.a', prefix/'lib/libpsl.a', prefix/'lib/libicuuc.a',
                     prefix/'lib/libodyssey-amigaos3.a', runtime/'lib/libstdc++.a',
                     toolchain/'m68k-amigaos/lib/libamisslstubs.a'):
        if not required.is_file():
            parser.error(f'missing dependency: {required}')
    archive = build/'downloads'/f'curl-{VERSION}.tar.xz'
    if not archive.exists():
        subprocess.run(['curl', '-fL', '--retry', '3', URL, '-o', str(archive)], check=True)
    if digest(archive) != SHA256:
        raise RuntimeError('curl archive SHA256 mismatch')
    source = build/f'curl-{VERSION}'
    if not source.exists():
        with tarfile.open(archive) as tar:
            tar.extractall(build, filter='data')
    # The SDK pthread header makes close(...) a cancellation macro. Keep
    # function-pointer member calls from accidentally expanding that macro.
    vtls = source/'lib/vtls/vtls.c'
    content = vtls.read_text()
    content = content.replace('Curl_ssl->close(cf, data);', '(Curl_ssl->close)(cf, data);')
    content = content.replace('connssl->ssl_impl->close(cf, data);', '(connssl->ssl_impl->close)(cf, data);')
    vtls.write_text(content)
    target = build/'target'
    compiler = str(toolchain/'bin/m68k-amigaos-gcc')
    env = dict(os.environ)
    env.update(CC=compiler, AR=str(toolchain/'bin/m68k-amigaos-ar'),
               RANLIB=str(toolchain/'bin/m68k-amigaos-ranlib'),
               CFLAGS='-m68020 -O2 -D_DEFAULT_SOURCE',
               # SDK bsdsocket headers require ssize_t before inclusion.
               CPPFLAGS=f'-include unistd.h -I{prefix}/include',
               LDFLAGS=f'-m68020 -L{runtime}/lib -L{prefix}/lib',
               LIBS='-licuuc -licudata -lstdc++ -lm -latomic -lpthread -lodyssey-amigaos3 -Wl,--wrap=getcwd,--wrap=utimes,--wrap=utime', PKG_CONFIG_PATH='',
               PKG_CONFIG_LIBDIR=str(prefix/'lib/pkgconfig'))
    if args.probes_only and not (prefix/'lib/libcurl.a').is_file():
        parser.error('--probes-only requires an already staged libcurl.a')
    history = (json.loads((build/'manifest.json').read_text()).get('commands', [])
        if args.probes_only and (build/'manifest.json').exists() else [])
    def run(argv, cwd=target, name='build'):
        argv = list(map(str, argv))
        history.append(argv)
        print(shlex.join(argv), flush=True)
        with (build/'logs'/f'{name}.log').open('w') as log:
            subprocess.run(argv, cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    if not args.probes_only:
        run([source/'configure', '--host=m68k-unknown-amigaos', f'--prefix={prefix}',
             '--disable-shared', '--enable-static', '--with-amissl', '--with-libpsl',
             '--without-libidn2', '--without-nghttp2', '--without-nghttp3',
             '--without-ngtcp2', '--without-libssh2', '--without-libssh',
             '--without-brotli', '--without-zstd', '--disable-ldap', '--disable-ldaps',
             '--disable-threaded-resolver', '--disable-unix-sockets', '--disable-ipv6',
             '--disable-docs', '--disable-manual', f'--with-zlib={prefix}'], name='configure')
        run(['make', '-C', 'lib', f'-j{args.jobs}'], name='library')
        run(['make', '-C', 'lib', 'install-libLTLIBRARIES'], name='install-library')
        run(['make', '-C', 'include/curl', 'install-pkgincludeHEADERS'], name='install-headers')
        libtool = prefix/'lib/libcurl.la'
        libtool.write_text(libtool.read_text().replace(' -lamisslauto', ''))
        # This describes actual AmiSSL import archives, never fake libssl/libcrypto.
        pkg = prefix/'lib/pkgconfig'
        pkg.mkdir(parents=True, exist_ok=True)
        (pkg/'libcurl.pc').write_text((target/'libcurl.pc').read_text().replace(' -lamisslauto', ''))
        (pkg/'amissl.pc').write_text(
            f'prefix={toolchain}/m68k-amigaos\nlibdir=${{prefix}}/lib\nincludedir=${{prefix}}/include\n\n'
            'Name: AmiSSL\nDescription: Native AmiSSL OpenSSL import stubs with explicit application task initialization\n'
            'Version: 5.27\nLibs: -L${libdir} -lamisslstubs\nCflags: -I${includedir}\n')
        licenses = prefix/'share/licenses/curl'
        licenses.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source/'COPYING', licenses/'COPYING')
    if args.smoke:
        flags = ['-m68020', '-O2']
        for name, src in [('curl-files', ROOT/'Tools/AmigaOS3/dependencies/curl-files.c'),
                          ('amissl-context', ROOT/'Source/WebCore/platform/network/curl/AmigaOS3AmiSSL.c'),
                          ('amiga-tty', ROOT/'Source/WTF/wtf/amigaos3/TTY.c'),
                          ('amiga-malloc', ROOT/'Source/WTF/wtf/amigaos3/Malloc.c')]:
            run([compiler, *flags, '-c', src, '-o', build/f'{name}.o'], name=name)
        run([compiler, *flags, f'-I{prefix}/include', ROOT/'Tools/AmigaOS3/curl-test.c',
             build/'curl-files.o', build/'amiga-malloc.o', build/'amiga-tty.o', build/'amissl-context.o', f'-L{runtime}/lib', f'-L{prefix}/lib',
             *(runtime/'lib'/name for name in math_helpers),
             '-lcurl', '-lamisslstubs', '-lz', '-lpsl', '-licuuc', '-licudata',
             '-lstdc++', '-lm', '-latomic', '-lpthread', '-lodyssey-amigaos3',
             '-Wl,--wrap=_malloc_r,--wrap=_free_r,--wrap=_calloc_r,--wrap=_realloc_r,--wrap=isatty',
             '-Wl,--wrap=getcwd,--wrap=utimes,--wrap=utime',
             '-o', build/'curl-test'], name='smoke-link')
    manifest = {'version': VERSION, 'url': URL, 'sha256': SHA256, 'toolchain': str(toolchain),
                'cxx_runtime': str(runtime), 'compiler_helpers': math_helpers, 'commands': history, 'environment': {k: env[k] for k in ('CC','CFLAGS','CPPFLAGS','LDFLAGS','LIBS','PKG_CONFIG_LIBDIR')},
                'libcurl_sha256': digest(prefix/'lib/libcurl.a'),
                'runtime_boundary': 'Application must initialize AmiSSL and bind native socket errno on the owning task.'}
    manifest['runtime_inputs'] = {str(path): digest(path) for path in
        [runtime/'lib'/name for name in math_helpers]
        + [runtime/'lib/libgcc.a', runtime/'lib/libstdc++.a', runtime/'lib/libpthread.a',
           toolchain/'m68k-amigaos/lib/libm020/libc.a',
           toolchain/'m68k-amigaos/lib/libamisslstubs.a']}
    if args.smoke:
        manifest['probe_sha256'] = digest(build/'curl-test')
    (build/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')


if __name__ == '__main__':
    main()
