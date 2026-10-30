#!/usr/bin/env python3
"""Build a pthread-enabled C++ runtime overlay without changing the SDK.

The SDK's --enable-threads=no runtime shares emulated TLS across tasks and
does not synchronize shared_ptr or local static initialization. Rebuild
libstdc++ and libgcc's emutls implementation against the SDK's pthread API.
The remaining SDK libgcc objects are retained for this exception-free port.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile


def run(arguments, *, cwd=None, env=None, log=None):
    if log:
        with Path(log).open("w") as output:
            subprocess.run(arguments, cwd=cwd, env=env, stdout=output,
                           stderr=subprocess.STDOUT, check=True)
    else:
        subprocess.run(arguments, cwd=cwd, env=env, check=True)


def install_c99_math(prefix):
    """Expose only C99 overloads backed by this SDK's actual libm ABI."""
    header = Path(__file__).resolve().parent / "runtime-include/bits/amigaos3_c99_math.h"
    installed = prefix / "include/c++/bits/amigaos3_c99_math.h"
    installed.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(header, installed)
    cmath = prefix / "include/c++/cmath"
    text = cmath.read_text()
    directive = "#include <bits/amigaos3_c99_math.h>\n"
    anchor = "#if _GLIBCXX_USE_STD_SPEC_FUNCS\n"
    if directive not in text:
        if text.count(anchor) != 1:
            raise SystemExit("Unsupported GCC cmath layout for native C99 adaptation")
        cmath.write_text(text.replace(anchor, directive + "\n" + anchor))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gcc-source", type=Path,
                        default=Path.home() / "git/AmigaPorts/gcc")
    parser.add_argument("--gcc-revision",
                        default="e141137a28e4c36e45c8dc9064f413ddb17099bd")
    parser.add_argument("--pthread-source", type=Path,
                        default=Path.home() / "old/git/m68k-amigaos-gcc/projects/aros-stuff")
    parser.add_argument("--pthread-revision",
                        default="86711c70569ffc6b70ee1fb99895866c1d9f30e4")
    parser.add_argument("--toolchain", type=Path, default=Path("/opt/amiga-16.2"))
    parser.add_argument("--build-dir", type=Path, default=Path("build/amigaos3-gthreads"))
    parser.add_argument("--prefix", type=Path, default=Path("build/amigaos3-sdk/cxx-runtime"))
    parser.add_argument("--jobs", type=int, default=8)
    args = parser.parse_args()
    root = args.build_dir.resolve()
    prefix = args.prefix.resolve()
    source = root / "source"
    build = root / "libstdcxx"
    libgcc = root / "libgcc"
    sdk_include = root / "sdk-include"
    for directory in (source, build, libgcc, sdk_include, prefix / "lib"):
        directory.mkdir(parents=True, exist_ok=True)

    gcc_repository = args.gcc_source.resolve()
    git = ["git", "-C", str(gcc_repository)]
    revision = subprocess.check_output(git + ["rev-parse", args.gcc_revision], text=True).strip()
    marker = root / "source-revision"
    if marker.exists() and marker.read_text().strip() != revision:
        raise SystemExit("Use a fresh build directory when changing GCC revision")
    if not marker.exists():
        files = ["libstdc++-v3", "libgcc", "libbacktrace", "libiberty", "config", "include",
                 "gcc/BASE-VER", "gcc/DATESTAMP", "gcc/DEV-PHASE", "config-ml.in",
                 "config.guess", "config.sub", "install-sh", "missing", "mkinstalldirs",
                 "ltmain.sh", "libtool-ldflags", "libtool.m4", "ltoptions.m4", "ltsugar.m4",
                 "ltversion.m4", "lt~obsolete.m4", "contrib/relpath.sh", "COPYING3", "COPYING.RUNTIME"]
        archive = subprocess.Popen(git + ["archive", revision, *files], stdout=subprocess.PIPE)
        with tarfile.open(fileobj=archive.stdout, mode="r|") as contents:
            contents.extractall(source, filter="data")
        if archive.wait():
            raise SystemExit("GCC source extraction failed")
        marker.write_text(revision + "\n")

    toolchain = args.toolchain.resolve()
    cc = str(toolchain / "bin/m68k-amigaos-gcc")
    cxx = str(toolchain / "bin/m68k-amigaos-g++")
    ar = str(toolchain / "bin/m68k-amigaos-ar")
    ranlib = str(toolchain / "bin/m68k-amigaos-ranlib")
    pthread_source = root / "pthread-source"
    pthread_source.mkdir(parents=True, exist_ok=True)
    pthread_git = ["git", "-C", str(args.pthread_source.resolve())]
    pthread_revision = subprocess.check_output(
        pthread_git + ["rev-parse", args.pthread_revision], text=True).strip()
    archive = subprocess.Popen(pthread_git + ["archive", pthread_revision, "pthreads"],
                               stdout=subprocess.PIPE)
    with tarfile.open(fileobj=archive.stdout, mode="r|") as contents:
        contents.extractall(pthread_source, filter="data")
    if archive.wait():
        raise SystemExit("pthread source extraction failed")
    pthread_patch = Path(__file__).resolve().parent / "patches/pthread-stable-records.patch"
    run(["patch", "-p1", "-i", str(pthread_patch)], cwd=pthread_source,
        log=root / "pthread-patch.log")
    # This source has the installed SDK's public ABI. Keep the other SDK
    # archive members (sched/semaphore), replacing the repaired implementation.
    if (pthread_source / "pthreads/pthread.h").read_bytes() != (
            toolchain / "m68k-amigaos/include/pthread.h").read_bytes():
        raise SystemExit("Pinned pthread header differs from the selected SDK ABI")
    run([cc, "-m68020", "-O2", "-I" + str(pthread_source / "pthreads"),
         "-c", str(pthread_source / "pthreads/pthread.c"), "-o", str(root / "pthread.o")])
    sdk_pthread = toolchain / "m68k-amigaos/lib/libm020/libpthread.a"
    shutil.copy2(sdk_pthread, prefix / "lib/libpthread.a")
    run([ar, "r", str(prefix / "lib/libpthread.a"), str(root / "pthread.o")])
    run([ranlib, str(prefix / "lib/libpthread.a")])
    shutil.copy2(source / "libgcc/gthr-posix.h", libgcc / "gthr-default.h")

    # fenv.h's field name collides with newlib's __unused attribute macro.
    # Preserve the SDK implementation and ABI, changing only the field name.
    fenv = (toolchain / "m68k-amigaos/sys-include/fenv.h").read_text()
    fenv = fenv.replace("unsigned int __unused;", "unsigned int __fenv_unused;")
    fenv = fenv.replace("envp->__unused", "envp->__fenv_unused")
    (sdk_include / "fenv.h").write_text(fenv)
    runtime_headers = Path(__file__).resolve().parent / "runtime-include"
    for header in runtime_headers.glob("*.h"):
        shutil.copy2(header, sdk_include / header.name)

    flags = "-m68020 -O2 -g0 -I" + str(sdk_include)
    environment = os.environ.copy()
    environment.update(CC=cc, CXX=cxx, AR=ar, RANLIB=ranlib,
                       NM=str(toolchain / "bin/m68k-amigaos-nm"),
                       AS=str(toolchain / "bin/m68k-amigaos-as"),
                       LD=str(toolchain / "bin/m68k-amigaos-ld"),
                       TARGET_C_FLAGS=flags, gcc_cv_target_thread_file="posix")
    arguments = [str(source / "libstdc++-v3/configure"), "--host=m68k-amigaos",
                 "--build=" + subprocess.check_output([str(source / "config.guess")], text=True).strip(),
                 "--prefix=" + str(prefix), "--libdir=" + str(prefix / "lib"),
                 "--with-gxx-include-dir=" + str(prefix / "include/c++"),
                 "--disable-multilib", "--disable-shared", "--disable-nls",
                 "--disable-libstdcxx-pch", "--disable-libstdcxx-backtrace",
                 "--with-libstdcxx-lock-policy=atomic", "--enable-libstdcxx-threads"]
    run(arguments, cwd=build, env=environment, log=root / "configure.log")
    # These functions are provided by WTF's native amigaos3/Time.c, rather
    # than the unmodified SDK libc used by configure's cross-link probes.
    # Select the real EClock monotonic clock and timer.device sleep backend.
    config = build / "config.h"
    configuration = config.read_text()
    for feature in ("_GLIBCXX_USE_CLOCK_MONOTONIC", "_GLIBCXX_USE_CLOCK_REALTIME", "_GLIBCXX_USE_NANOSLEEP"):
        configuration = configuration.replace("/* #undef " + feature + " */", "#define " + feature + " 1")
    config.write_text(configuration)
    # libstdc++ does not track all generated-header dependencies. Rebuild
    # after changing target features so chrono/guard/TLS objects agree with
    # the installed c++config.h even when reusing a build directory.
    run(["make", "clean"], cwd=build, env=environment, log=root / "clean.log")
    generated_config = build / "include/m68k-amigaos/bits/c++config.h"
    # BSD make compares second-resolution timestamps. configure can create
    # this header in the same second as the feature edits above.
    if generated_config.exists():
        os.utime(generated_config, (0, 0))
    run(["make", "-j" + str(args.jobs)], cwd=build, env=environment, log=root / "make.log")
    if "#define _GLIBCXX_USE_CLOCK_MONOTONIC 1" not in generated_config.read_text():
        raise SystemExit("libstdc++ generated configuration lost the native clock selection")
    chrono_symbols = subprocess.check_output(
        [environment["NM"], "-u", str(build / "src/c++11/chrono.o")], text=True)
    if "_clock_gettime" not in chrono_symbols:
        raise SystemExit("libstdc++ chrono object does not use the native clock")
    run(["make", "install"], cwd=build, env=environment, log=root / "install.log")
    install_c99_math(prefix)

    # emutls.c needs no target-specific overrides for this m68k ABI. Supply
    # the usual C declarations and select the real GCC POSIX thread adapter.
    for name in ("tconfig.h", "coretypes.h", "tm.h", "libgcc_tm.h"):
        (libgcc / name).write_text("/* Standalone emutls: no target overrides. */\n")
    (libgcc / "tsystem.h").write_text("#include <stdlib.h>\n#include <string.h>\n")
    run([cc, "-m68020", "-O2", "-DGTHREAD_USE_WEAK=0", "-I" + str(libgcc),
         "-I" + str(source / "libgcc"), "-c", str(source / "libgcc/emutls.c"),
         "-o", str(libgcc / "emutls.o")])
    original_libgcc = Path(subprocess.check_output([cc, "-m68020", "-print-libgcc-file-name"], text=True).strip())
    shutil.copy2(original_libgcc, prefix / "lib/libgcc.a")
    run([ar, "r", str(prefix / "lib/libgcc.a"), str(libgcc / "emutls.o")])
    run([ranlib, str(prefix / "lib/libgcc.a")])
    shutil.copy2(libgcc / "emutls.o", prefix / "lib/emutls.o")
    # The SDK libc overrides GCC's unsigned floating-point helpers with Amiga
    # math-library wrapper that adds 2^27 instead of 2^31 for high-bit inputs.
    # Link the pinned GCC implementation as an object before libc.
    # Its double-to-float wrapper also truncates through IEEEDPTieee instead
    # of using GCC's round-to-nearest-even conversion.
    for helper in ("floatunsidf", "floatunsisf", "truncdfsf2"):
        run([cc, "-m68020", "-O2", "-D__" + helper.upper(), "-c",
             str(source / "libgcc/config/m68k/fpgnulib.c"),
             "-o", str(prefix / "lib" / (helper + ".o"))])
    # Native mathieeedoubbas arithmetic saturates double overflow and does
    # not supply IEEE NaN semantics. Use GCC's complete software double
    # arithmetic/comparison set, including its default-nearest exception
    # bookkeeping. The SDK comparison object aliases all six predicates,
    # so replacing only arithmetic would pull conflicting cmpdf2 symbols.
    double_sections = ("double", "floatex", "eqdf2", "nedf2", "gtdf2", "gedf2", "ltdf2", "ledf2")
    run([cc, "-m68020", *["-DL_" + section for section in double_sections], "-c",
         str(source / "libgcc/config/m68k/lb1sf68.S"),
         "-o", str(prefix / "lib/double-math.o")])
    shutil.copy2(sdk_include / "fenv.h", prefix / "include/fenv.h")
    for header in runtime_headers.glob("*.h"):
        shutil.copy2(header, prefix / "include" / header.name)
    license_directory = prefix / "share/gcc-runtime"
    license_directory.mkdir(parents=True, exist_ok=True)
    for name in ("COPYING3", "COPYING.RUNTIME"):
        shutil.copy2(source / name, license_directory / name)
    pthread_distribution = prefix / "share/amiga-pthread"
    pthread_distribution.mkdir(parents=True, exist_ok=True)
    for name in ("pthread.c", "pthread.h"):
        shutil.copy2(pthread_source / "pthreads" / name, pthread_distribution / name)
    (prefix / "manifest.json").write_text(json.dumps({
        "gcc_revision": revision,
        "gcc_repository": "https://github.com/AmigaPorts/gcc",
        "pthread_revision": pthread_revision,
        "pthread_repository": "https://github.com/AmigaPorts/aros-stuff",
        "compiler": subprocess.check_output([cc, "--version"], text=True).splitlines()[0],
        "cpu": "68020", "thread_model": "posix", "shared_ptr_lock_policy": "atomic",
        "libgcc_rebuilt_members": ["emutls.o"],
        "compiler_helper_objects": ["floatunsidf.o", "floatunsisf.o", "truncdfsf2.o", "double-math.o"],
        "required_platform_sources": ["Source/WTF/wtf/amigaos3/Time.c", "Source/WTF/wtf/amigaos3/Malloc.c"],
        "cxx_include_directories": [str(prefix / "include/c++/m68k-amigaos"), str(prefix / "include/c++")],
        "library_directory": str(prefix / "lib"),
        "sha256": {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in {
            "recipe": Path(__file__),
            "include/c++/cmath": prefix / "include/c++/cmath",
            "include/c++/bits/amigaos3_c99_math.h": prefix / "include/c++/bits/amigaos3_c99_math.h",
            "libgcc/emutls.c": source / "libgcc/emutls.c",
            "libgcc/gthr-posix.h": source / "libgcc/gthr-posix.h",
            "lib/libstdc++.a": prefix / "lib/libstdc++.a",
            "lib/libsupc++.a": prefix / "lib/libsupc++.a",
            "lib/libgcc.a": prefix / "lib/libgcc.a",
            "lib/emutls.o": prefix / "lib/emutls.o",
            "lib/floatunsidf.o": prefix / "lib/floatunsidf.o",
            "lib/floatunsisf.o": prefix / "lib/floatunsisf.o",
            "lib/truncdfsf2.o": prefix / "lib/truncdfsf2.o",
            "lib/double-math.o": prefix / "lib/double-math.o",
            "libgcc/config/m68k/lb1sf68.S": source / "libgcc/config/m68k/lb1sf68.S",
            "libgcc/config/m68k/fpgnulib.c": source / "libgcc/config/m68k/fpgnulib.c",
            "lib/libpthread.a": prefix / "lib/libpthread.a",
            "pthread.c": pthread_source / "pthreads/pthread.c",
            "pthread_patch": pthread_patch,
            "sdk/libpthread.a": sdk_pthread,
        }.items()},
        "limitations": ["C++17 application runtime; GCC C++23 module interfaces are not validated",
                        "Other SDK libgcc members are retained; application C++ exceptions are disabled"]
    }, indent=2) + "\n")
    print("C++ runtime overlay installed:", prefix)


if __name__ == "__main__":
    main()
