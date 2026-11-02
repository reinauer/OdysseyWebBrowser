#!/usr/bin/env python3
"""Stage a local AmigaOS3 installation without modifying any build inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

NEWLIB_URL = ('https://raw.githubusercontent.com/AmigaPorts/newlib-cygwin/'
              'fb0a76ceacae0c7c1442d5a1e00be6ba7f669de2/COPYING.NEWLIB')
# 1980-01-01 UTC, matching the fixed timestamp of the ZIP entries.
ARCHIVE_TIME = 315532800


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write_checksum(archive):
    with archive.with_name(archive.name+'.sha256').open('x') as stream:
        stream.write(digest(archive)+'  '+archive.name+'\n')


def write_lha(archive, output, names):
    """Pack for LhA 2.15, the native tool which extraction was tested with.

    Generic headers leave out the builder's UID; -lh5- suits slow CPUs.
    """
    for name in names:
        os.utime(output/name, (ARCHIVE_TIME, ARCHIVE_TIME))
    environment = dict(os.environ, TZ='UTC')
    subprocess.run(['lha', 'cqgo5', str(archive), *names], cwd=output, env=environment, check=True)
    subprocess.run(['lha', 'tq', str(archive)], env=environment, check=True, stdout=subprocess.DEVNULL)
    write_checksum(archive)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'source-root', 'dependency-root', 'runtime-root',
                 'pthread-sources', 'newlib-license', 'output'):
        parser.add_argument('--'+name, required=True, type=Path)
    parser.add_argument('--newlib-license-url', default=NEWLIB_URL)
    parser.add_argument('--archive', type=Path, help='Optional deterministic ZIP; must not exist')
    parser.add_argument('--lha-archive', type=Path, help='Optional deterministic LHA; must not exist')
    args = parser.parse_args()
    for name, value in vars(args).items():
        if isinstance(value, Path):
            setattr(args, name, value.absolute())
    output = args.output
    if output.exists() or output.is_symlink():
        parser.error('output already exists: '+str(output))
    for archive in (args.archive, args.lha_archive):
        if not archive:
            continue
        if archive.exists() or archive.is_symlink():
            parser.error('archive already exists: '+str(archive))
        if output.resolve() in archive.resolve().parents:
            parser.error('archive must be outside the staged output directory')
        checksum = archive.with_name(archive.name+'.sha256')
        if checksum.exists() or checksum.is_symlink():
            parser.error('archive checksum already exists: '+str(checksum))
    if args.lha_archive and not shutil.which('lha'):
        parser.error('--lha-archive requires lha on PATH')
    roots = [args.source_root, args.dependency_root, args.runtime_root, args.pthread_sources]
    for root in roots:
        if not root.is_dir():
            parser.error('missing input directory: '+str(root))
        if root.resolve() == output.resolve() or output.resolve() in root.resolve().parents:
            parser.error('output must not contain an input directory')
    with args.binary.open('rb') as binary:
        if binary.read(4) != b'\x00\x00\x03\xf3':
            parser.error('binary is not a native Amiga HUNK executable')

    # Preflight every source before creating the new output directory.
    copies = []
    inputs = {}

    def add_file(source, target):
        if source.is_symlink() or not source.is_file():
            raise RuntimeError('expected a regular input file: '+str(source))
        copies.append((source, Path(target)))
        inputs[str(source.resolve())] = digest(source)

    def add_tree(source, target, exclude=None):
        if not source.is_dir():
            raise RuntimeError('missing input tree: '+str(source))
        for path in sorted(source.rglob('*')):
            if path.is_symlink():
                raise RuntimeError('symlink input is not supported: '+str(path))
            if exclude and exclude in path.parents:
                continue
            if path.is_file():
                if path.suffix.lower() in ('.library', '.mcc', '.mcp', '.datatype'):
                    raise RuntimeError('OS/MUI runtime file is not a bundle asset: '+str(path))
                add_file(path, Path(target)/path.relative_to(source))

    # The OS3 executable links its ICU data; these other platforms' files are
    # most of the bundle's file count and would only slow native extraction.
    add_tree(args.source_root/'Dist', '.', args.source_root/'Dist/Resource/icudt49b')
    add_tree(args.dependency_root/'share/amigaos3', '.')
    # Select the same relocatable font configuration used by native tests.
    add_file(args.dependency_root/'share/amigaos3/Conf/fontconfig/fonts.conf',
             'Conf/font/fonts.conf')
    add_file(args.dependency_root/'share/amigaos3/Fonts/NotoSans-LICENSE',
             'Licenses/NotoSans-LICENSE')
    add_tree(args.dependency_root/'share/licenses', 'Licenses/dependencies')
    # These notices were previously omitted by case-sensitive source globs.
    for relative in ('libxml2/Copyright', 'libxslt/Copyright', 'freetype/FTL.TXT',
                     'freetype/GPLv2.TXT', 'sqlite/PUBLIC-DOMAIN.txt'):
        if not (args.dependency_root/'share/licenses'/relative).is_file():
            raise RuntimeError('dependency notice missing; restage licenses: '+relative)
    add_tree(args.runtime_root/'share/gcc-runtime', 'Licenses/gcc-runtime')
    add_file(args.runtime_root/'manifest.json', 'Build/runtime-manifest.json')
    add_file(args.newlib_license, 'Licenses/newlib/COPYING.NEWLIB')
    add_file(args.source_root/'Tools/AmigaOS3/INSTALL.txt', 'AmigaOS3.readme')
    for relative in ('LICENSE.LGPL', 'Source/JavaScriptCore/COPYING.LIB',
                     'Source/WebCore/LICENSE-LGPL-2.1', 'Source/WebCore/LICENSE-LGPL-2',
                     'Source/WebCore/LICENSE-APPLE', 'Source/WebKitLegacy/LICENSE',
                     'Source/WTF/LICENSE-libc++.txt'):
        add_file(args.source_root/relative, Path('Licenses/Odyssey')/relative)
    for path in sorted((args.source_root/'Source/WebInspectorUI/UserInterface/External').rglob('LICENSE*')):
        if path.is_file():
            add_file(path, Path('Licenses/Inspector')/path.relative_to(
                args.source_root/'Source/WebInspectorUI/UserInterface/External'))
    # Preserve each leading source notice verbatim, including author credits.
    pthread_notices = {}
    for path in sorted(args.pthread_sources.glob('*')):
        if path.suffix in ('.c', '.h'):
            data = path.read_bytes()
            if data.startswith(b'/*') and b'*/' in data:
                pthread_notices[path.name+'.txt'] = data[:data.index(b'*/')+2]+b'\n'
                inputs[str(path.resolve())] = digest(path)
    if not pthread_notices:
        raise RuntimeError('no pthread source notices found')
    add_file(args.binary, 'OWB')
    inputs[str(Path(__file__).resolve())] = digest(Path(__file__))
    for root in (args.dependency_root/'lib', args.runtime_root/'lib'):
        for path in sorted(root.glob('*')):
            if path.suffix in ('.a', '.o'):
                inputs[str(path.resolve())] = digest(path)

    output.mkdir(parents=True, exist_ok=False)
    bundle = output/'Odyssey'
    bundle.mkdir()
    for source, target in copies:
        destination = bundle/target
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        destination.chmod(0o644)
        if digest(destination) != inputs[str(source.resolve())]:
            raise RuntimeError('input changed while staging: '+str(source))
    (bundle/'OWB').chmod(0o755)
    for name, notice in pthread_notices.items():
        destination = bundle/'Licenses/pthread'/name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(notice)
    commit = subprocess.check_output(['git', '-C', str(args.source_root),
                                      'rev-parse', 'HEAD'], text=True).strip()
    diff = subprocess.check_output(['git', '-C', str(args.source_root), 'diff', 'HEAD', '--binary'])
    manifest = {
        'format': 1, 'source_commit': commit,
        'source_tracked_diff_sha256': hashlib.sha256(diff).hexdigest(),
        'source_tracked_dirty': bool(diff),
        'inputs': inputs, 'newlib_license_url': args.newlib_license_url,
        'binary_sha256': digest(bundle/'OWB'),
        'files': {str(path.relative_to(bundle)): digest(path)
                  for path in sorted(bundle.rglob('*')) if path.is_file()},
        'note': 'Local installation bundle; runtime validation is bound to the binary hash.'}
    (bundle/'Build/inputs.json').write_text(json.dumps(manifest, indent=2, sort_keys=True)+'\n')
    paths = [path for path in sorted(bundle.rglob('*')) if path.is_file()]
    (bundle/'SHA256SUMS').write_text(''.join(
        digest(path)+'  '+str(path.relative_to(bundle))+'\n' for path in paths))
    if args.archive:
        with zipfile.ZipFile(args.archive, 'x', compression=zipfile.ZIP_DEFLATED,
                             compresslevel=9) as archive:
            for path in sorted(bundle.rglob('*')):
                if path.is_file():
                    entry = zipfile.ZipInfo(str(path.relative_to(output)), (1980, 1, 1, 0, 0, 0))
                    entry.create_system = 3
                    entry.external_attr = (0o100755 if path.name == 'OWB' else 0o100644) << 16
                    entry.compress_type = zipfile.ZIP_DEFLATED
                    archive.writestr(entry, path.read_bytes(), compresslevel=9)
        write_checksum(args.archive)
    if args.lha_archive:
        # Workbench only shows the extracted drawer when it has an icon.
        shutil.copyfile(args.source_root/'Dist/Documentation.info', output/'Odyssey.info')
        write_lha(args.lha_archive, output, ['Odyssey.info']+[
            str(path.relative_to(output)) for path in sorted(bundle.rglob('*')) if path.is_file()])
    print('Staged '+str(bundle))
    print('OWB SHA256 '+manifest['binary_sha256'])


if __name__ == '__main__':
    main()
