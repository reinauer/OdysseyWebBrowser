#!/usr/bin/env python3
"""Build pinned browser dependencies for the native m68k compiler SDK.

All downloads, builds and installation stay below the Odyssey checkout.
The staged prefix is shared with build-icu.py; this script never replaces it.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PACKAGES = {
    'zlib': ('1.3.1', 'https://zlib.net/fossils/zlib-1.3.1.tar.gz', '9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23'),
    'libpng': ('1.6.47', 'https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.47.tar.gz', '631a4c58ea6c10c81f160c4b21fa8495b715d251698ebc2552077e8450f30454'),
    'freetype': ('2.12.1', 'https://download.savannah.gnu.org/releases/freetype/freetype-2.12.1.tar.xz', '4766f20157cc4cf0cd292f80bf917f92d1c439b243ac3018debf6b9140c41a7f'),
    'pixman': ('0.40.0', 'https://cairographics.org/releases/pixman-0.40.0.tar.gz', '6d200dec3740d9ec4ec8d1180e25779c00bc749f94278c8b9021f5534db223fc'),
    'expat': ('2.7.1', 'https://github.com/libexpat/libexpat/releases/download/R_2_7_1/expat-2.7.1.tar.xz', '354552544b8f99012e5062f7d570ec77f14b412a3ff5c7d8d0dae62c0d217c30'),
    'fontconfig': ('2.14.2', 'https://www.freedesktop.org/software/fontconfig/release/fontconfig-2.14.2.tar.xz', 'dba695b57bce15023d2ceedef82062c2b925e51f5d4cc4aef736cf13f60a468b'),
    'cairo': ('1.16.0', 'https://cairographics.org/releases/cairo-1.16.0.tar.xz', '5e7b29b3f113ef870d1e3ecf8adf21f923396401604bda16d44be45e66052331'),
    'libxml2': ('2.12.10', 'https://download.gnome.org/sources/libxml2/2.12/libxml2-2.12.10.tar.xz', 'c3d8c0c34aa39098f66576fe51969db12a5100b956233dc56506f7a8679be995'),
    'libxslt': ('1.1.43', 'https://download.gnome.org/sources/libxslt/1.1/libxslt-1.1.43.tar.xz', '5a3d6b383ca5afc235b171118e90f5ff6aa27e9fea3303065231a6d403f0183a'),
    'harfbuzz': ('8.3.0', 'https://github.com/harfbuzz/harfbuzz/releases/download/8.3.0/harfbuzz-8.3.0.tar.xz', '109501eaeb8bde3eadb25fab4164e993fbace29c3d775bcaa1c1e58e2f15f847'),
    'libwebp': ('1.5.0', 'https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.5.0.tar.gz', '7d6fab70cf844bf6769077bd5d7a74893f8ffd4dfb42861745750c63c2a5c92c'),
    'libjpeg-turbo': ('3.1.0', 'https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.0/libjpeg-turbo-3.1.0.tar.gz', '9564c72b1dfd1d6fe6274c5f95a8d989b59854575d4bbee44ade7bc17aa9bc93'),
    'libpsl': ('0.21.5', 'https://github.com/rockdaboot/libpsl/releases/download/0.21.5/libpsl-0.21.5.tar.gz', '1dcc9ceae8b128f3c0b3f654decd0e1e891afc6ff81098f227ef260449dae208'),
    'sqlite': ('3.46.1', 'https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip', '77823cb110929c2bcb0f5d48e4833b5c59a8a6e40cdea3936b99e199dbbe5784'),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def patch(path, before, after):
    content = path.read_text()
    if after in content:
        return
    if before in content:
        path.write_text(content.replace(before, after))
    else:
        raise RuntimeError(f'Patch context changed: {path}')


def fontconfig_paths(src):
    patch(src/'src/fcint.h', "#  define FC_SEARCH_PATH_SEPARATOR ':'",
          "#  define FC_SEARCH_PATH_SEPARATOR ';' /* Amiga assign names contain colon. */")
    patch(src/'src/fcstr.c', "    return *s == '/';", "    return strchr ((const char *)s, ':') != NULL;")
    patch(src/'src/fcstr.c', 'if (p != ret)\n', "if (p != ret && p[-1] != ':')\n")
    patch(src/'src/fcstr.c', '    if (s[0] == \'/\')\n', '    if (FcStrIsAbsoluteFilename (s))\n')
    strings = src/'src/fcstr.c'
    content = strings.read_text()
    start = content.index('static FcChar8 *\nFcStrCanonAbsoluteFilename ')
    end = content.index('\n#ifdef _WIN32\n/*\n * Convert', start)
    content = content[:start] + ('static FcChar8 *\nFcStrCanonAbsoluteFilename (const FcChar8 *s)\n'
        '{\n    /* Amiga double slash means parent directory; never squash it. */\n'
        '    return FcStrdup (s);\n}\n') + content[end:]
    strings.write_text(content)
    patch(src/'src/fcstr.c', "    slash = (FcChar8 *) strrchr ((const char *) path, '/');",
          "    slash = (FcChar8 *) strrchr ((const char *) path, '/');\n    if (!slash) slash = (FcChar8 *) strrchr ((const char *) path, ':');")
    patch(src/'src/fcstr.c', '    dir = malloc ((slash - file) + 1);',
          "    if (*slash == ':') ++slash;\n    dir = malloc ((slash - file) + 1);")
    patch(src/'src/fccfg.c', "path[strlen((char *) path)-1] != '/') && file[0] != '/')",
          "path[strlen((char *) path)-1] != '/' && path[strlen((char *) path)-1] != ':') && file[0] != '/')")
    patch(src/'src/fcdir.c', '    strcat ((char *) file_prefix, FC_DIR_SEPARATOR_S);',
          "    if (file_prefix[strlen ((char *)file_prefix)-1] != ':')\n        strcat ((char *) file_prefix, FC_DIR_SEPARATOR_S);")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--toolchain', type=Path, default=Path('/opt/amiga-16.2'))
    p.add_argument('--build-root', type=Path, default=ROOT/'build/amigaos3-deps-other')
    p.add_argument('--prefix', type=Path, default=ROOT/'build/amigaos3-sdk')
    p.add_argument('--cxx-runtime', type=Path, help='Native pthread C++ runtime overlay (default: PREFIX/cxx-runtime)')
    p.add_argument('--jobs', type=int, default=4)
    p.add_argument('--probes-only', action='store_true', help='Relink requested probes using already staged dependencies')
    p.add_argument('--smoke', action='store_true', help='Link the native graphics/XML smoke executable')
    p.add_argument('--font-smoke', action='store_true', help='Stage a licensed test font and link the native text smoke')
    p.add_argument('--sqlite-smoke', action='store_true', help='Link the native SQLite transaction smoke')
    p.add_argument('--sqlite-volume', default='RAM:', help='Guest volume for the SQLite smoke (RAM: or DH1:)')
    p.add_argument('--psl-smoke', action='store_true', help='Link the native public-suffix smoke')
    p.add_argument('--file-smoke', action='store_true', help='Link the native filesystem metadata smoke')
    p.add_argument('--file-volume', default='RAM:', help='Guest volume for the filesystem smoke (RAM: or DH1:)')
    p.add_argument('packages', nargs='*', choices=list(PACKAGES))
    a = p.parse_args()
    build, prefix, toolchain = a.build_root.resolve(), a.prefix.resolve(), a.toolchain.resolve()
    for d in (build/'downloads', build/'src', build/'logs', prefix/'include', prefix/'lib/pkgconfig'):
        d.mkdir(parents=True, exist_ok=True)
    target = toolchain/'m68k-amigaos'
    env = dict(os.environ)
    env.update(PATH=str(toolchain/'bin')+os.pathsep+env['PATH'],
        CC=str(toolchain/'bin/m68k-amigaos-gcc'), CXX=str(toolchain/'bin/m68k-amigaos-g++'),
        AR=str(toolchain/'bin/m68k-amigaos-ar'), RANLIB=str(toolchain/'bin/m68k-amigaos-ranlib'),
        NM=str(toolchain/'bin/m68k-amigaos-nm'), STRIP=str(toolchain/'bin/m68k-amigaos-strip'),
        CFLAGS='-m68020 -O2 -D_DEFAULT_SOURCE', CXXFLAGS='-m68020 -O2 -D_DEFAULT_SOURCE',
        CPPFLAGS=f'-I{prefix}/include', LDFLAGS=f'-m68020 -L{prefix}/lib',
        PKG_CONFIG_LIBDIR=str(prefix/'lib/pkgconfig'), PKG_CONFIG_PATH='', PKG_CONFIG_SYSROOT_DIR='')
    runtime = (a.cxx_runtime or prefix/'cxx-runtime').resolve()
    runtime_link_flags = []
    helper_objects = [runtime/'lib'/name for name in
        ('floatunsidf.o', 'floatunsisf.o', 'truncdfsf2.o', 'double-math.o')]
    if (runtime/'lib/integer-math.o').exists():
        helper_objects.append(runtime/'lib/integer-math.o')
    requested_probes = a.smoke or a.font_smoke or a.sqlite_smoke or a.psl_smoke or a.file_smoke
    if a.probes_only and (not requested_probes or a.packages):
        raise RuntimeError('--probes-only requires probe flags and no package names')
    if requested_probes and any(not path.exists() for path in helper_objects):
        raise RuntimeError('Rebuild the native runtime: all four compiler helper objects are required for probes')
    if (runtime/'lib/libstdc++.a').exists():
        includes = [runtime/'include', runtime/'include/c++/m68k-amigaos', runtime/'include/c++']
        env['CXXFLAGS'] += ' -nostdinc++ ' + ' '.join('-isystem '+shlex.quote(str(d)) for d in includes)
        runtime_link_flags = [f'-L{runtime}/lib']
        env['LDFLAGS'] += ' '+shlex.join(runtime_link_flags)
    elif not a.probes_only and 'harfbuzz' in (a.packages or list(PACKAGES)):
        raise RuntimeError('Build the native pthread C++ runtime before HarfBuzz')
    manifest = {'compiler': subprocess.check_output([env['CC'],'--version'],text=True).splitlines()[0],
                'cpu': '68020', 'packages': {}, 'sdk': {}}
    manifest['sdk'][str(target/'lib/libm020/libc.a')] = sha(target/'lib/libm020/libc.a')
    manifest['build_script_sha256'] = sha(Path(__file__))
    manifest['cxx_runtime'] = {str(path):sha(path) for path in
        [runtime/'lib/libgcc.a', runtime/'lib/libstdc++.a', runtime/'lib/libsupc++.a'] + helper_objects if path.exists()}
    manifest['support_sources'] = {str(path.relative_to(ROOT)):sha(path) for path in
        [ROOT/'Source/WTF/wtf/amigaos3/Time.c', ROOT/'Source/WTF/wtf/amigaos3/Random.c']
        + list((ROOT/'Tools/AmigaOS3/dependencies').glob('*.c'))}
    symbols = subprocess.check_output([env['NM'],str(target/'lib/libm020/libc.a')],stderr=subprocess.DEVNULL,text=True)
    if ' T ___check_fno' not in symbols:
        raise RuntimeError('SDK lacks the verified newlib DOS descriptor accessor')
    if (build/'manifest.json').exists():
        manifest['packages'] = json.loads((build/'manifest.json').read_text())['packages']

    def run(command, cwd, label):
        print(label, flush=True)
        with (build/'logs'/f'{label}.log').open('w') as log:
            log.write('COMMAND: '+repr([str(v) for v in command])+'\n'); log.flush()
            subprocess.run(command,cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)

    if not a.probes_only:
        run([env['CC'], '-m68020', '-O2', '-c', str(ROOT/'Source/WTF/wtf/amigaos3/Time.c'), '-o', str(build/'Time.o')], build, 'native-time')
        run([env['CC'], '-m68020', '-O2', '-c', str(ROOT/'Tools/AmigaOS3/dependencies/dirent.c'), '-o', str(build/'dirent.o')], build, 'native-dirent')
        run([env['CC'], '-m68020', '-O2', '-c', str(ROOT/'Tools/AmigaOS3/dependencies/files.c'), '-o', str(build/'files.o')], build, 'native-files')
        run([env['CC'], '-m68020', '-O2', '-c', str(ROOT/'Source/WTF/wtf/amigaos3/Random.c'), '-o', str(build/'Random.o')], build, 'native-random')
        run([env['CC'], '-m68020', '-O2', '-c', str(ROOT/'Tools/AmigaOS3/dependencies/inet_pton.c'), '-o', str(build/'inet_pton.o')], build, 'native-inet-pton')
        run([env['CC'], '-m68020', '-O2', '-c', str(ROOT/'Tools/AmigaOS3/dependencies/curl-files.c'), '-o', str(build/'curl-files.o')], build, 'native-curl-files')
        shutil.copy2(ROOT/'Tools/AmigaOS3/dependencies/include/dirent.h', prefix/'include/dirent.h')
        run([env['AR'], 'rcs', str(prefix/'lib/libodyssey-amigaos3.a'), str(build/'Time.o'), str(build/'dirent.o'), str(build/'files.o'), str(build/'Random.o'), str(build/'inet_pton.o'), str(build/'curl-files.o')], build, 'native-support')
        (prefix/'share/licenses/musl').mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT/'Tools/AmigaOS3/dependencies/MUSL-COPYRIGHT',prefix/'share/licenses/musl/COPYRIGHT')
    support_wrappers = '-Wl,--wrap=getcwd,--wrap=utimes,--wrap=utime'
    env['LIBS'] = '-latomic -lpthread -lodyssey-amigaos3 ' + support_wrappers
    base_env = env.copy()

    for name in ([] if a.probes_only else (a.packages or list(PACKAGES))):
        env = base_env.copy()
        version, url, checksum = PACKAGES[name]
        archive = build/'downloads'/('libpng-1.6.47.tar.gz' if name == 'libpng' else url.rsplit('/',1)[-1])
        if not archive.exists():
            run(['curl','-fL',url,'-o',str(archive)],build,name+'-download')
        if sha(archive) != checksum:
            raise RuntimeError(f'{archive}: SHA256 mismatch')
        src = build/'src'/('sqlite-amalgamation-3460100' if name == 'sqlite' else f'{name}-{version}')
        if not src.exists():
            if archive.suffix == '.zip':
                with zipfile.ZipFile(archive) as zip:
                    zip.extractall(build/'src')
            else:
                with tarfile.open(archive) as tar:
                    tar.extractall(build/'src',filter='data')
        obj = build/f'{name}-build'; obj.mkdir(exist_ok=True)
        common = [str(src/'configure'),'--host=m68k-unknown-amigaos','--build=aarch64-apple-darwin',f'--prefix={prefix}','--disable-shared','--enable-static']
        if name == 'zlib':
            env['CHOST'] = 'm68k-amigaos'
            run([str(src/'configure'),'--static',f'--prefix={prefix}'],obj,name+'-configure')
            (obj/'libz.a').unlink(missing_ok=True)
            run(['make',f'-j{a.jobs}','libz.a'],obj,name+'-build')
            run(['make','install'],obj,name+'-install')
        elif name in ['libpng', 'freetype']:
            if name == 'libpng':
                cmake = src/'CMakeLists.txt'
                cmake.write_text(cmake.read_text().replace('if(UNIX\n   OR (WIN32', 'if(CMAKE_SYSTEM_PROCESSOR STREQUAL "m68k"\n   OR UNIX\n   OR (WIN32'))
            options = ['-DPNG_SHARED=OFF','-DPNG_STATIC=ON','-DPNG_TESTS=OFF','-DPNG_TOOLS=OFF'] if name == 'libpng' else ['-DCMAKE_POLICY_VERSION_MINIMUM=3.5','-DFT_DISABLE_BZIP2=ON','-DFT_DISABLE_HARFBUZZ=ON','-DFT_DISABLE_BROTLI=ON']
            run(['cmake','-S',str(src),'-B',str(obj),'-G','Ninja',f'-DCMAKE_TOOLCHAIN_FILE={ROOT}/Source/cmake/AmigaOS3.cmake',f'-DAMIGAOS3_TOOLCHAIN_ROOT={toolchain}',f'-DAMIGAOS3_DEPENDENCY_ROOT={prefix}',f'-DCMAKE_INSTALL_PREFIX={prefix}','-DBUILD_SHARED_LIBS=OFF','-DCMAKE_BUILD_TYPE=Release']+options,obj,name+'-configure')
            run(['cmake','--build',str(obj),'--parallel',str(a.jobs)],obj,name+'-build')
            run(['cmake','--install',str(obj)],obj,name+'-install')
        elif name == 'pixman':
            env['ac_cv_tls'] = 'none'
            # Upstream mixes int*/int32_t* in raster loops. Both are 32-bit
            # in this ABI; disable alias assumptions for these legacy loops.
            env['CFLAGS'] += ' -fno-strict-aliasing -Wno-error=incompatible-pointer-types'
            # Amiga stdint uses long for int32_t; preserve pointer types.
            header = src/'pixman/pixman-inlines.h'
            original = header.read_text()
            header.write_text(original.replace('int width1 = *width, left_pad1, right_pad1;', 'int32_t width1 = *width, left_pad1, right_pad1;').replace('int width2 = *width, left_pad2, right_pad2;', 'int32_t width2 = *width, left_pad2, right_pad2;'))
            args = ['--disable-openmp','--disable-gtk','--disable-libpng']
            run(common+args,obj,name+'-configure')
            run(['make',f'-j{a.jobs}','-C','pixman'],obj,name+'-build')
            run(['make','-C','pixman','install'],obj,name+'-install')
            shutil.copy2(obj/'pixman-1.pc',prefix/'lib/pkgconfig/pixman-1.pc')
        elif name == 'expat':
            args = ['--without-docbook','--without-tests','--without-examples','--without-xmlwf','--without-getrandom','--without-sys-getrandom']
            run(common+args,obj,name+'-configure')
            run(['make',f'-j{a.jobs}'],obj,name+'-build')
            run(['make','install'],obj,name+'-install')
        elif name == 'fontconfig':
            fontconfig_paths(src)
            env['ac_cv_func_random'] = 'no'  # SDK has random, but not initstate/setstate.
            compat = src/'src/fccompat.c'
            content = compat.read_text().replace('mkstemp (char *template)', 'FcPrivateMkstemp (char *template)')
            if '#define mkstemp FcPrivateMkstemp' not in content:
                content = content.replace('#define HAVE_MKSTEMP 1', '#define HAVE_MKSTEMP 1\n#define mkstemp FcPrivateMkstemp')
            compat.write_text(content)
            glue = src/'src/ftglue.c'
            glue.write_text(glue.read_text().replace('stream->read(', '(stream->read)('))
            env['EXPAT_LIBS'] = f'-L{prefix}/lib -lexpat -lodyssey-amigaos3'
            args = ['--disable-docs','--disable-nls','--disable-cache-build','--disable-iconv','--with-default-fonts=PROGDIR:Fonts,FONTS:_TrueType','--with-cache-dir=T:fontconfig','--with-baseconfigdir=PROGDIR:Conf/fontconfig','--with-configdir=PROGDIR:Conf/fontconfig/conf.d']
            run(common+args,obj,name+'-configure')
            run(['make',f'-j{a.jobs}','-C','src'],obj,name+'-build')
            run(['make','-C','src','install'],obj,name+'-install')
            run(['make','-C','fontconfig','install'],obj,name+'-headers')
            shutil.copy2(obj/'fontconfig.pc',prefix/'lib/pkgconfig/fontconfig.pc')
            runtime_config = prefix/'share/amigaos3/Conf/fontconfig'
            runtime_config.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT/'Tools/AmigaOS3/dependencies/fonts.conf', runtime_config/'fonts.conf')
        elif name == 'cairo':
            # SDK newlib's fortified inline declarations are incomplete.
            configure = src/'configure'
            configure.write_text(configure.read_text().replace('MAYBE_WARN="$MAYBE_WARN -Wp,-D_FORTIFY_SOURCE=2"', ''))
            env['PTHREAD_LIBS'] = '-lpthread -lodyssey-amigaos3'
            args = ['--disable-xlib','--disable-xcb','--disable-gl','--disable-egl','--disable-glesv2','--disable-qt','--disable-quartz','--disable-quartz-font','--disable-quartz-image','--disable-tee','--disable-interpreter','--disable-trace','--disable-symbol-lookup','--enable-ft','--enable-fc','--enable-png']
            run(common+args,obj,name+'-configure')
            run(['make',f'-j{a.jobs}','-C','src'],obj,name+'-build')
            run(['make','-C','src','install'],obj,name+'-install')
        elif name == 'libxml2':
            run(common+['--without-python','--without-lzma','--without-iconv','--without-http','--without-ftp','--without-zlib'],obj,name+'-configure')
            run(['make',f'-j{a.jobs}','libxml2.la'],obj,name+'-build')
            run(['make','install-libLTLIBRARIES','install-pkgconfigDATA'],obj,name+'-install')
            run(['make','-C','include','install'],obj,name+'-headers')
        elif name == 'libxslt':
            run(common+['--without-python','--without-crypto'],obj,name+'-configure')
            for subdir in ['libxslt','libexslt']:
                run(['make',f'-j{a.jobs}','-C',subdir],obj,name+'-'+subdir+'-build')
                run(['make','-C',subdir,'install'],obj,name+'-'+subdir+'-install')
            run(['make','install-pkgconfigDATA'],obj,name+'-install')
        elif name == 'harfbuzz':
            env['PTHREAD_LIBS'] = '-lpthread -lodyssey-amigaos3'
            env['FREETYPE_LIBS'] = f'-L{prefix}/lib -lfreetype -lpng -lz'
            run(common+['--with-glib=no','--with-gobject=no','--with-cairo=no','--with-icu=yes','--with-freetype=yes','--without-graphite2'],obj,name+'-configure')
            run(['make','-C','src','clean'],obj,name+'-clean')
            run(['make',f'-j{a.jobs}','-C','src'],obj,name+'-build')
            run(['make','-C','src','install'],obj,name+'-install')
        elif name == 'libwebp':
            run(common+['--disable-threading','--disable-sdl','--disable-gl','--disable-png','--disable-jpeg','--disable-tiff','--disable-gif','--disable-libwebpextras'],obj,name+'-configure')
            run(['make',f'-j{a.jobs}','-C','sharpyuv'],obj,name+'-sharpyuv')
            run(['make','-C','sharpyuv','install'],obj,name+'-sharpyuv-install')
            run(['make',f'-j{a.jobs}','-C','src'],obj,name+'-build')
            run(['make','-C','src','install'],obj,name+'-install')
        elif name == 'libjpeg-turbo':
            run(['cmake','-S',str(src),'-B',str(obj),'-G','Ninja',f'-DCMAKE_TOOLCHAIN_FILE={ROOT}/Source/cmake/AmigaOS3.cmake',f'-DAMIGAOS3_TOOLCHAIN_ROOT={toolchain}',f'-DCMAKE_INSTALL_PREFIX={prefix}','-DENABLE_SHARED=OFF','-DENABLE_STATIC=ON','-DWITH_SIMD=OFF','-DWITH_TURBOJPEG=OFF','-DCMAKE_BUILD_TYPE=Release'],obj,name+'-configure')
            run(['cmake','--build',str(obj),'--target','jpeg-static','--parallel',str(a.jobs)],obj,name+'-build')
            for component in ['lib','include']:
                run(['cmake','--install',str(obj),'--component',component],obj,name+'-install-'+component)
        elif name == 'libpsl':
            patch(src/'src/psl.c', 'static int isip(const char *hostname)', 'extern int inet_pton(int, const char *, void *);\nstatic int isip(const char *hostname)')
            run(common+['--enable-runtime=libicu','--enable-builtin','--disable-gtk-doc','--disable-man'],obj,name+'-configure')
            run(['make',f'-j{a.jobs}','-C','src'],obj,name+'-build')
            run(['make','-C','src','install'],obj,name+'-install')
            run(['make','-C','include','install'],obj,name+'-headers')
            shutil.copy2(obj/'libpsl.pc',prefix/'lib/pkgconfig/libpsl.pc')
            with (prefix/'lib/pkgconfig/libpsl.pc').open('a') as pc:
                pc.write('\nRequires.private: icu-uc\nLibs.private: -lstdc++ -lm -latomic -lpthread -lodyssey-amigaos3\n')
        elif name == 'sqlite':
            patch(src/'sqlite3.c','#if SQLITE_THREADSAFE && !defined(SQLITE_MUTEX_NOOP)', '#if SQLITE_THREADSAFE && !defined(SQLITE_MUTEX_NOOP) && !defined(SQLITE_MUTEX_PTHREADS)')
            flags = ['-m68020','-O2','-D_DEFAULT_SOURCE','-DSQLITE_OS_OTHER=1','-DSQLITE_THREADSAFE=1','-DSQLITE_MUTEX_PTHREADS','-DSQLITE_OMIT_LOAD_EXTENSION','-DSQLITE_OMIT_WAL','-DSQLITE_TEMP_STORE=2','-DSQLITE_MAX_MMAP_SIZE=0',f'-I{src}']
            run([env['CC']]+flags+['-c',str(src/'sqlite3.c'),'-o',str(obj/'sqlite3.o')],obj,name+'-build')
            run([env['CC']]+flags+['-c',str(ROOT/'Tools/AmigaOS3/dependencies/sqlite-vfs.c'),'-o',str(obj/'sqlite-vfs.o')],obj,name+'-vfs')
            run([env['AR'],'rcs',str(prefix/'lib/libsqlite3.a'),str(obj/'sqlite3.o'),str(obj/'sqlite-vfs.o')],obj,name+'-archive')
            for header in ['sqlite3.h','sqlite3ext.h']:
                shutil.copy2(src/header,prefix/'include'/header)
            (prefix/'lib/pkgconfig/sqlite3.pc').write_text(f'prefix={prefix}\nlibdir=${{prefix}}/lib\nincludedir=${{prefix}}/include\nName: SQLite\nDescription: SQLite with native AmigaDOS rollback-journal VFS\nVersion: {version}\nLibs: -L${{libdir}} -lsqlite3\nLibs.private: -lpthread -lodyssey-amigaos3 -lm\nCflags: -I${{includedir}}\n')
        manifest['packages'][name] = {'version':version,'url':url,'sha256':checksum}
        libraries = {'zlib':'z','libpng':'png','freetype':'freetype','pixman':'pixman-1',
            'expat':'expat','fontconfig':'fontconfig','cairo':'cairo','libxml2':'xml2',
            'libxslt':'xslt','harfbuzz':'harfbuzz','libwebp':'webp','libjpeg-turbo':'jpeg','libpsl':'psl','sqlite':'sqlite3'}
        library = prefix/'lib'/f'lib{libraries[name]}.a'
        if not subprocess.check_output([env['AR'],'t',str(library)],text=True).strip():
            raise RuntimeError(f'Empty target archive: {library}')
        manifest['packages'][name]['archive_sha256'] = sha(library)
        if name == 'harfbuzz':
            icu_library = prefix/'lib/libharfbuzz-icu.a'
            if not subprocess.check_output([env['AR'],'t',str(icu_library)],text=True).strip():
                raise RuntimeError(f'Empty target archive: {icu_library}')
            manifest['packages'][name]['icu_archive_sha256'] = sha(icu_library)
        license_dir = prefix/'share/licenses'/name
        license_dir.mkdir(parents=True, exist_ok=True)
        for pattern in ['COPYING*', 'LICENSE*', 'AUTHORS*']:
            for license_file in src.glob(pattern):
                if license_file.is_file():
                    shutil.copy2(license_file, license_dir/license_file.name)
        pc_names = {'pixman':'pixman-1','expat':'expat','fontconfig':'fontconfig','cairo':'cairo','harfbuzz':'harfbuzz','libxml2':'libxml-2.0','libxslt':'libxslt'}
        if name in pc_names:
            pc = prefix/'lib/pkgconfig'/f'{pc_names[name]}.pc'
            content = pc.read_text()
            support = '-latomic -lpthread -lodyssey-amigaos3'
            if support not in content:
                if 'Libs.private:' in content:
                    content = re.sub(r'(?m)^(Libs.private:.*)$', r'\1 '+support, content)
                else:
                    content += '\nLibs.private: '+support+'\n'
                pc.write_text(content)
        (build/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    # Current newlib bundles these public names with unrelated ABI helpers.
    # Wrap the native implementations instead of exporting duplicate symbols.
    if not a.probes_only:
        for pc in (prefix/'lib/pkgconfig').glob('*.pc'):
            content = pc.read_text()
            if '-lodyssey-amigaos3' in content and support_wrappers not in content:
                pc.write_text(content.replace('-lodyssey-amigaos3', '-lodyssey-amigaos3 ' + support_wrappers))
    print(f'Staged libraries in {prefix}',flush=True)
    probe_link_flags = runtime_link_flags + [str(path) for path in helper_objects]
    if requested_probes:
        for unit in ('Malloc', 'TTY'):
            source = ROOT/'Source/WTF/wtf/amigaos3'/f'{unit}.c'
            run([env['CC'], '-m68020', '-O2', '-I'+str(runtime/'include'), '-c', str(source),
                 '-o', str(build/f'probe-{unit}.o')], build, f'native-probe-{unit.lower()}')
            probe_link_flags.append(str(build/f'probe-{unit}.o'))
            manifest['support_sources'][str(source.relative_to(ROOT))] = sha(source)
        probe_link_flags.append(
            '-Wl,--wrap=_malloc_r,--wrap=_free_r,--wrap=_calloc_r,--wrap=_realloc_r,--wrap=isatty')
        probe_link_flags.append(support_wrappers)
    if a.smoke:
        flags = subprocess.check_output(['pkg-config','--static','--cflags','--libs','cairo','libxslt'],env=env,text=True).split()
        run([env['CC'],'-m68020','-O2',str(ROOT/'Tools/AmigaOS3/dependencies/smoke.c'),'-o',str(build/'dependency-smoke')]+probe_link_flags+flags+['-latomic','-lpthread','-lodyssey-amigaos3'],build,'dependency-smoke-link')
    if a.font_smoke:
        commit = 'ffebf8c1ee449e544955a7e813c54f9b73848eac'
        font_dir = prefix/'share/amigaos3/Fonts'
        font_dir.mkdir(parents=True, exist_ok=True)
        for file, remote, checksum in [
            ('NotoSans-Regular.ttf','hinted/ttf/NotoSans/NotoSans-Regular.ttf','b85c38ecea8a7cfb39c24e395a4007474fa5a4fc864f6ee33309eb4948d232d5'),
            ('NotoSans-LICENSE','LICENSE','0dab92d0544f7b233403f14b84a663bdbfa746982eda629e7f4f9ffe1b036feb')]:
            archive = build/'downloads'/file
            if not archive.exists():
                run(['curl','-fL',f'https://raw.githubusercontent.com/notofonts/noto-fonts/{commit}/{remote}','-o',str(archive)],build,file+'-download')
            if sha(archive) != checksum:
                raise RuntimeError(f'{archive}: SHA256 mismatch')
            shutil.copy2(archive,font_dir/file)
        flags = subprocess.check_output(['pkg-config','--static','--cflags','--libs','harfbuzz-icu','cairo'],env=env,text=True).split()
        run([env['CC'],'-m68020','-O2',str(ROOT/'Tools/AmigaOS3/dependencies/font-smoke.c'),'-o',str(build/'font-smoke')]+probe_link_flags+flags+['-latomic','-lpthread','-lodyssey-amigaos3','-lstdc++'],build,'font-smoke-link')
    for enabled, package, source, extra in [
        (a.sqlite_smoke,'sqlite3','sqlite-smoke', [f'-DSQLITE_SMOKE_VOLUME="{a.sqlite_volume}"']),
        (a.psl_smoke,'libpsl','psl-smoke', [])]:
        if enabled:
            flags = subprocess.check_output(['pkg-config','--static','--cflags','--libs',package],env=env,text=True).split()
            run([env['CC'],'-m68020','-O2']+extra+[str(ROOT/f'Tools/AmigaOS3/dependencies/{source}.c'),'-o',str(build/source)]+probe_link_flags+flags+['-latomic','-lpthread','-lodyssey-amigaos3','-lstdc++','-lm'],build,source+'-link')

    if a.file_smoke:
        run([env['CC'], '-m68020', '-O2', f'-DFILE_SMOKE_VOLUME="{a.file_volume}"',
             str(ROOT/'Tools/AmigaOS3/dependencies/file-smoke.c'), '-o', str(build/'file-smoke')]
            + probe_link_flags + ['-L'+str(prefix/'lib'), '-lodyssey-amigaos3', '-latomic', '-lpthread'],
            build, 'file-smoke-link')

    manifest['probes'] = {name: sha(build/name) for enabled, name in [
        (a.smoke, 'dependency-smoke'), (a.font_smoke, 'font-smoke'),
        (a.sqlite_smoke, 'sqlite-smoke'), (a.psl_smoke, 'psl-smoke'),
        (a.file_smoke, 'file-smoke')] if enabled}
    (build/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')


if __name__ == '__main__':
    main()
