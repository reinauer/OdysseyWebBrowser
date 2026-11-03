#!/usr/bin/env python3
"""Build isolated baseline/fixed curl socket errno regression probes.

Uses an existing build-curl.py installation. No SDK, curl source tree or
installed archive is modified, and no server or emulator is launched.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile

import amigaos3_cpu

ROOT = Path(__file__).resolve().parents[2]
CURL_VERSION = '8.23.0'
CURL_SHA256 = '8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path, default=Path('/opt/amiga-16.2'))
    parser.add_argument('--prefix', type=Path, default=ROOT/'build/amigaos3-sdk')
    parser.add_argument('--cxx-runtime', type=Path)
    parser.add_argument('--curl-build', type=Path, default=ROOT/'build/amigaos3-deps-curl')
    parser.add_argument('--output', type=Path, required=True,
                        help='new private directory; existing directories are rejected')
    amigaos3_cpu.add_argument(parser)
    args = parser.parse_args()
    sdk, prefix, build, output = (p.resolve() for p in
                                  (args.toolchain, args.prefix, args.curl_build, args.output))
    runtime = (args.cxx_runtime or prefix/'cxx-runtime').resolve()
    source = build/f'curl-{CURL_VERSION}'
    curl_archive = prefix/'lib/libcurl.a'
    download = build/'downloads'/f'curl-{CURL_VERSION}.tar.xz'
    manifest_path = build/'manifest.json'
    required = [sdk/'bin/m68k-amigaos-gcc', source/'lib/cf-socket.c',
                source/'lib/curl_setup.h', build/'target/lib/curl_config.h',
                download, curl_archive, manifest_path, runtime/'manifest.json']
    for path in required:
        if not path.is_file():
            parser.error(f'missing input: {path}')
    manifest = json.loads(manifest_path.read_text())
    if (digest(download) != CURL_SHA256 or manifest.get('sha256') != CURL_SHA256
            or manifest.get('version') != CURL_VERSION):
        parser.error('curl source archive does not match the pinned 8.23.0 release')
    if manifest.get('socket_error_channel') != 'native-bsdsocket-task-owned-v1':
        parser.error('rebuild curl with task-owned socket errors first')
    if manifest.get('libcurl_sha256') != digest(curl_archive):
        parser.error('installed curl archive differs from its build manifest')
    if Path(manifest['toolchain']).resolve() != sdk:
        parser.error('use the toolchain recorded in the curl build manifest')
    if Path(manifest['cxx_runtime']).resolve() != runtime:
        parser.error('use the runtime recorded in the curl build manifest')
    if output.exists():
        parser.error('output already exists; use a fresh directory to preserve evidence')

    original = (source/'lib/cf-socket.c').read_text()
    with tarfile.open(download) as archive:
        pinned_source = archive.extractfile(f'curl-{CURL_VERSION}/lib/cf-socket.c').read().decode()
    if original != pinned_source:
        parser.error('cf-socket.c differs from the pinned release; review it before instrumenting')
    receive = '  rv = sread(ctx->sock, buf, len);'
    setup = '#include "curl_setup.h"'
    if original.count(receive) != 1 or original.count(setup) != 1:
        parser.error('curl receive source changed; review the instrumentation points')
    instrumented = original.replace(receive, receive +
        '\n  extern void odysseySocketErrnoProbeAfterReceive(long);'
        '\n  odysseySocketErrnoProbeAfterReceive(rv);')
    instrumented = instrumented.replace(setup, setup + '''
#ifdef SOCKET_ERRNO_PROBE_BASELINE
#undef SOCKERRNO
#undef SET_SOCKERRNO
#define SOCKERRNO errno
#define SET_SOCKERRNO(value) (errno = (value))
#endif
''')
    output.mkdir(parents=True)
    private_source = output/'cf-socket-probe.c'
    private_source.write_text(instrumented)
    commands = []

    def run(command):
        command = [str(part) for part in command]
        commands.append(command)
        result = subprocess.run(command, capture_output=True, text=True)
        with (output/'build.log').open('a') as log:
            log.write(result.stdout + result.stderr)
        (output/'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        if result.returncode:
            raise RuntimeError(f'compiler failed; see {output / "build.log"}')

    cc = sdk/'bin/m68k-amigaos-gcc'
    flags = [*amigaos3_cpu.flags(args.cpu), '-O2', '-D_DEFAULT_SOURCE', '-I'+str(prefix/'include')]
    support = {
        'amissl-context': ROOT/'Source/WebCore/platform/network/curl/AmigaOS3AmiSSL.c',
        'malloc': ROOT/'Source/WTF/wtf/amigaos3/Malloc.c',
        'tty': ROOT/'Source/WTF/wtf/amigaos3/TTY.c',
        'curl-files': ROOT/'Tools/AmigaOS3/dependencies/curl-files.c',
    }
    for name, path in support.items():
        run([cc, *flags, '-c', path, '-o', output/(name+'.o')])
    helpers = [runtime/'lib'/name for name in
               (*amigaos3_cpu.math_helpers(args.cpu), 'integer-math.o')]
    for mode in ('baseline', 'fixed'):
        definitions = ['-DSOCKET_ERRNO_PROBE_BASELINE'] if mode == 'baseline' else []
        socket_object = output/f'cf-socket-{mode}.o'
        run([cc, *flags, *definitions, '-pthread', '-DHAVE_CONFIG_H',
             '-DBUILDING_LIBCURL', '-include', 'unistd.h',
             '-I'+str(source/'include'), '-I'+str(build/'target/lib'),
             '-I'+str(source/'lib'), '-c', private_source, '-o', socket_object])
        run([cc, *flags, *definitions, ROOT/'Tools/AmigaOS3/SocketErrnoTest.c',
             socket_object, *[output/(name+'.o') for name in support],
             '-L'+str(runtime/'lib'), '-L'+str(prefix/'lib'), *helpers,
             '-lcurl', '-lamisslstubs', '-lz', '-lpsl', '-licuuc', '-licudata',
             '-lstdc++', '-lm', '-latomic', '-lpthread', '-lodyssey-amigaos3',
             '-Wl,--wrap=_malloc_r,--wrap=_free_r,--wrap=_calloc_r,--wrap=_realloc_r,--wrap=isatty',
             '-Wl,--wrap=getcwd,--wrap=utimes,--wrap=utime',
             '-o', output/f'socket-errno-{mode}'])

    inputs = [*required, *support.values(), ROOT/'Tools/AmigaOS3/SocketErrnoTest.c',
              Path(__file__).resolve(), *helpers]
    identity = {
        'curl_version': CURL_VERSION,
        'boundary': 'Private actual curl receive instrumentation; baseline restores '
                    'the former global errno binding and macro only in this probe.',
        'inputs': {str(path): digest(path) for path in inputs},
        'outputs': {path.name: digest(path) for path in output.iterdir()
                    if path.is_file()},
    }
    (output/'identity.json').write_text(json.dumps(identity, indent=2) + '\n')
    print(f'Built baseline and fixed probes in {output}')


if __name__ == '__main__':
    main()
