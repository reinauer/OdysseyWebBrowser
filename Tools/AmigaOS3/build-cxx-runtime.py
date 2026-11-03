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

import amigaos3_cpu


def run(arguments, *, cwd=None, env=None, log=None):
    if log:
        with Path(log).open("w") as output:
            subprocess.run(arguments, cwd=cwd, env=env, stdout=output,
                           stderr=subprocess.STDOUT, check=True)
    else:
        subprocess.run(arguments, cwd=cwd, env=env, check=True)



def sdk_fingerprint(toolchain, cpu_flags=("-m68020",)):
    """Identify the actual compiler, startup, libraries and searched headers.

    A release string alone is insufficient: SDK archives can be repackaged
    without changing gcc --version. Resolve the SDK path once, but hash
    relative identities so moving identical SDK bytes does not change it.
    """
    toolchain = toolchain.resolve()
    cc = toolchain / "bin/m68k-amigaos-gcc"
    cxx = toolchain / "bin/m68k-amigaos-g++"

    def output(compiler, *arguments):
        return subprocess.check_output([str(compiler), *arguments], text=True).strip()

    def name(path):
        path = path.resolve()
        try:
            return str(path.relative_to(toolchain))
        except ValueError:
            return str(path)

    def checksum(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()

    files = {"gcc": cc, "g++": cxx}
    for program in ("cc1", "cc1plus", "as", "ld"):
        compiler = cxx if program == "cc1plus" else cc
        reported = output(compiler, "-print-prog-name=" + program)
        resolved = shutil.which(reported) if "/" not in reported else reported
        if not resolved:
            raise SystemExit("Compiler tool cannot be resolved through PATH: " + reported)
        files[program] = Path(resolved)
    for program in ("ar", "ranlib"):
        files[program] = toolchain / ("bin/m68k-amigaos-" + program)
    for library in ("newlib-crt0.o", "libc.a", "libm.a", "libstubs.a",
                    "libpthread.a", "libgcc.a", "libstdc++.a", "libsupc++.a"):
        files[library] = Path(output(cxx, *cpu_flags, "-print-file-name=" + library))
    for label, path in files.items():
        if not path.is_file():
            raise SystemExit("SDK input cannot be located: " + label + " (" + str(path) + ")")

    include_roots = []
    for compiler, language in ((cc, "c"), (cxx, "c++")):
        probe = subprocess.run([str(compiler), *cpu_flags, "-E", "-v", "-x", language, "-"],
                               input="", text=True, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, check=True,
                               env=dict(os.environ, LC_ALL="C"))
        in_search = False
        for line in probe.stderr.splitlines():
            if line == "#include <...> search starts here:":
                in_search = True
            elif line == "End of search list.":
                in_search = False
            elif in_search:
                path = Path(line.strip()).resolve()
                if not path.is_dir():
                    raise SystemExit("SDK include directory cannot be located: " + str(path))
                if path not in include_roots:
                    include_roots.append(path)
    if not include_roots:
        raise SystemExit("Compiler did not report its target include search paths")
    headers = {}
    for directory in include_roots:
        digest = hashlib.sha256()
        count = 0
        for path in sorted(directory.rglob("*")):
            if path.is_file():
                digest.update(str(path.relative_to(directory)).encode() + b"\0")
                digest.update(bytes.fromhex(checksum(path)))
                count += 1
        headers[name(directory)] = {"files": count, "sha256": digest.hexdigest()}
    identity = {
        "target": output(cc, "-dumpmachine"),
        "compiler_version": output(cc, "--version").splitlines()[0],
        "multilib": output(cc, *cpu_flags, "-print-multi-directory"),
        "specs_sha256": hashlib.sha256(output(cc, "-dumpspecs").encode()).hexdigest(),
        "files": {label: {"path": name(path), "sha256": checksum(path)}
                  for label, path in sorted(files.items())},
        "include_search_order": [name(path) for path in include_roots],
        "include_trees": headers,
    }
    digest = hashlib.sha256(json.dumps(identity, sort_keys=True,
                                      separators=(",", ":")).encode()).hexdigest()
    return {"format": 1, "sha256": digest, "resolved_toolchain": str(toolchain),
            "resolved_inputs": {label: str(path.resolve()) for label, path in sorted(files.items())},
            "identity": identity}



def pthread_archive_compatibility(fingerprint, profile="legacy"):
    members = subprocess.check_output(
        [fingerprint["resolved_inputs"]["ar"], "t",
         fingerprint["resolved_inputs"]["libpthread.a"]], text=True).splitlines()
    expected = ["pthread.o", "sched.o", "semaphore.o"]
    if profile == "oct29":
        expected = ["pthread_attr.o", "pthread_barrier.o", "pthread_cancel.o",
                    "pthread_cond.o", "pthread_core.o", "pthread_key.o",
                    "pthread_mutex.o", "pthread_once.o", "pthread_rwlock.o",
                    "pthread_spin.o", "pthread_thread.o", "sched.o", "semaphore.o"]
    return {"supported": sorted(members) == expected,
            "actual_members": members, "expected_members": expected}


def check_sdk_directory(directory, fingerprint):
    marker = directory / "sdk-fingerprint.json"
    if marker.exists():
        recorded = json.loads(marker.read_text())
        if recorded.get("format") != fingerprint["format"] or recorded.get("sha256") != fingerprint["sha256"] or recorded.get("runtime_profile") != fingerprint.get("runtime_profile"):
            raise SystemExit("SDK fingerprint changed for " + str(directory)
                             + ". Use fresh --build-dir and --prefix directories; "
                             "existing runtime and native evidence were not changed.")
    elif directory.exists() and any(directory.iterdir()):
        raise SystemExit("Existing directory has no SDK fingerprint: " + str(directory)
                         + ". Use fresh --build-dir and --prefix directories; "
                         "do not relabel old outputs as validated for this SDK.")


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
    parser.add_argument("--profile", choices=("legacy", "oct29"), default="legacy",
                        help="select the separately pinned monolithic or Oct29 split pthread recipe")
    parser.add_argument("--gcc-source", type=Path,
                        default=Path.home() / "git/AmigaPorts/gcc")
    parser.add_argument("--gcc-revision",
                        default=None)
    parser.add_argument("--pthread-source", type=Path,
                        default=Path.home() / "old/git/m68k-amigaos-gcc/projects/aros-stuff")
    parser.add_argument("--pthread-revision",
                        default=None)
    parser.add_argument("--toolchain", type=Path, default=Path("/opt/amiga-16.2"))
    parser.add_argument("--build-dir", type=Path, default=Path("build/amigaos3-gthreads"))
    parser.add_argument("--prefix", type=Path, default=Path("build/amigaos3-sdk/cxx-runtime"))
    parser.add_argument("--jobs", type=int, default=8)
    amigaos3_cpu.add_argument(parser)
    parser.add_argument("--print-sdk-fingerprint", action="store_true",
                        help="print current SDK content identity without building or changing files")
    parser.add_argument("--sdk-fingerprint",
                        help="require this SHA256 SDK identity, including on a fresh build")
    args = parser.parse_args()
    args.gcc_revision = args.gcc_revision or ("1e6714c8f3756fc92ec93d4a3a25a49333b5407b" if args.profile == "oct29" else "e141137a28e4c36e45c8dc9064f413ddb17099bd")
    args.pthread_revision = args.pthread_revision or ("bf81f53fe4a3f309d8ebabaa893a1c740c39b3de" if args.profile == "oct29" else "86711c70569ffc6b70ee1fb99895866c1d9f30e4")
    toolchain = args.toolchain.resolve()
    cpu_flags = amigaos3_cpu.flags(args.cpu)
    soft_float = amigaos3_cpu.soft_float(args.cpu)
    fingerprint = sdk_fingerprint(toolchain, cpu_flags)
    fingerprint["requested_toolchain"] = str(args.toolchain.absolute())
    fingerprint["runtime_profile"] = args.profile if soft_float else args.profile + "-" + args.cpu
    compatibility = pthread_archive_compatibility(fingerprint, args.profile)
    fingerprint["pthread_recipe_compatibility"] = compatibility
    if args.print_sdk_fingerprint:
        print(json.dumps(fingerprint, indent=2))
        return
    if args.sdk_fingerprint and args.sdk_fingerprint != fingerprint["sha256"]:
        raise SystemExit("Selected SDK does not match --sdk-fingerprint: " + fingerprint["sha256"])
    if not compatibility["supported"]:
        raise SystemExit("Unsupported SDK pthread archive composition: "
                         + ", ".join(compatibility["actual_members"])
                         + ". Profile " + args.profile + " requires "
                         + ", ".join(compatibility["expected_members"])
                         + ". No build/prefix files were changed.")
    pinned = {"soft": "047486a65ed6ae2711feedbd12187671e03edafe88d3e1c2963396b4f2f38823",
              "fpu": "4232fd40e97a74491fc39ceec48f65a6f296e3ac51734a2c9cab655922eaab99"}
    if args.profile == "oct29" and fingerprint["sha256"] != pinned[args.cpu]:
        raise SystemExit("The Oct29 profile requires its pinned SDK content identity; inspect a new SDK before adding another profile")
    root = args.build_dir.resolve()
    prefix = args.prefix.resolve()
    # Check both before creating markers or modifying either tree.
    for directory in (root, prefix):
        check_sdk_directory(directory, fingerprint)
    for directory in (root, prefix):
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "sdk-fingerprint.json").write_text(json.dumps(fingerprint, indent=2) + "\n")
    cc = str(toolchain / "bin/m68k-amigaos-gcc")
    cxx = str(toolchain / "bin/m68k-amigaos-g++")
    ar = str(toolchain / "bin/m68k-amigaos-ar")
    ranlib = str(toolchain / "bin/m68k-amigaos-ranlib")
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
    pthread_patch = Path(__file__).resolve().parent / ("patches/pthread-oct29-tls.patch" if args.profile == "oct29" else "patches/pthread-stable-records.patch")
    run(["patch", "-p1", "-i", str(pthread_patch)], cwd=pthread_source,
        log=root / "pthread-patch.log")
    # This source has the installed SDK's public ABI. Keep the other SDK
    # archive members (sched/semaphore), replacing the repaired implementation.
    if (pthread_source / "pthreads/pthread.h").read_bytes() != (
            toolchain / "m68k-amigaos/include/pthread.h").read_bytes():
        raise SystemExit("Pinned pthread header differs from the selected SDK ABI")
    pthread_members = ["pthread_key", "pthread_cond"] if args.profile == "oct29" else ["pthread"]
    pthread_flags = (["-m68020-60", *([] if soft_float else ["-m68881"]),
                      "-Os", "-fomit-frame-pointer", "-noixemul"]
                     if args.profile == "oct29" else [*cpu_flags, "-O2"])
    pthread_objects = []
    for member in pthread_members:
        object_path = root / (member + ".o")
        run([cc, *pthread_flags, "-I" + str(pthread_source / "pthreads"),
             "-c", str(pthread_source / "pthreads" / (member + ".c")),
             "-o", str(object_path)])
        pthread_objects.append(str(object_path))
    sdk_pthread = Path(fingerprint["resolved_inputs"]["libpthread.a"])
    shutil.copy2(sdk_pthread, prefix / "lib/libpthread.a")
    run([ar, "r", str(prefix / "lib/libpthread.a"), *pthread_objects])
    # Every unmodified SDK member must be byte-identical, with no duplicate
    # or legacy implementation accidentally added to the split archive.
    installed_members = subprocess.check_output([ar, "t", str(prefix / "lib/libpthread.a")], text=True).splitlines()
    if installed_members != compatibility["actual_members"]:
        raise SystemExit("Repaired pthread archive changed member composition")
    for member in installed_members:
        if member not in [name + ".o" for name in pthread_members]:
            before = subprocess.check_output([ar, "p", str(sdk_pthread), member])
            after = subprocess.check_output([ar, "p", str(prefix / "lib/libpthread.a"), member])
            if before != after:
                raise SystemExit("Unrelated pthread archive member changed: " + member)
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

    flags = " ".join(cpu_flags) + " -O2 -g0 -I" + str(sdk_include)
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
    if sdk_fingerprint(toolchain, cpu_flags)["sha256"] != fingerprint["sha256"]:
        raise SystemExit("SDK contents changed during the build; stopping before libstdc++ install. "
                         "Partial artifacts were retained; use fresh build/prefix directories.")
    run(["make", "install"], cwd=build, env=environment, log=root / "install.log")
    install_c99_math(prefix)

    # emutls.c needs no target-specific overrides for this m68k ABI. Supply
    # the usual C declarations and select the real GCC POSIX thread adapter.
    for name in ("tconfig.h", "coretypes.h", "tm.h", "libgcc_tm.h"):
        (libgcc / name).write_text("/* Standalone emutls: no target overrides. */\n")
    (libgcc / "tsystem.h").write_text("#include <stdlib.h>\n#include <string.h>\n")
    run([cc, *cpu_flags, "-O2", "-DGTHREAD_USE_WEAK=0", "-I" + str(libgcc),
         "-I" + str(source / "libgcc"), "-c", str(source / "libgcc/emutls.c"),
         "-o", str(libgcc / "emutls.o")])
    original_libgcc = Path(subprocess.check_output([cc, *cpu_flags, "-print-libgcc-file-name"], text=True).strip())
    shutil.copy2(original_libgcc, prefix / "lib/libgcc.a")
    run([ar, "r", str(prefix / "lib/libgcc.a"), str(libgcc / "emutls.o")])
    run([ranlib, str(prefix / "lib/libgcc.a")])
    shutil.copy2(libgcc / "emutls.o", prefix / "lib/emutls.o")
    # The Oct14 SDK unsigned-to-double helper adds 2^27 instead of 2^31;
    # Oct29 fixes that typo. Both profiles retain canonical GCC helpers for
    # consistent tested conversion semantics, as explicit objects before libc.
    # Its double-to-float wrapper also truncates through IEEEDPTieee instead
    # of using GCC's round-to-nearest-even conversion.
    # FPU code performs these operations inline and keeps its results in fp0,
    # so the software helpers below belong only to the software profile.
    for helper in (("floatunsidf", "floatunsisf", "truncdfsf2") if soft_float else ()):
        run([cc, "-m68020", "-O2", "-D__" + helper.upper(), "-c",
             str(source / "libgcc/config/m68k/fpgnulib.c"),
             "-o", str(prefix / "lib" / (helper + ".o"))])
    # Native mathieeedoubbas arithmetic saturates double overflow and does
    # not supply IEEE NaN semantics. Use GCC's complete software double
    # arithmetic/comparison set, including its default-nearest exception
    # bookkeeping. The SDK comparison object aliases all six predicates,
    # so replacing only arithmetic would pull conflicting cmpdf2 symbols.
    double_sections = ("double", "floatex", "eqdf2", "nedf2", "gtdf2", "gedf2", "ltdf2", "ledf2")
    if soft_float:
        run([cc, "-m68020", *["-DL_" + section for section in double_sections], "-c",
             str(source / "libgcc/config/m68k/lb1sf68.S"),
             "-o", str(prefix / "lib/double-math.o")])
    integer_sections = ("divsi3", "modsi3", "udivsi3", "umodsi3", "mulsi3")
    # Integer sections reuse local labels when assembled together. Compile
    # them separately, then retain one explicit relocatable object for linking.
    integer_objects = []
    for section in integer_sections:
        object_path = libgcc / (section + ".o")
        run([cc, *cpu_flags, "-DL_" + section, "-c",
             str(source / "libgcc/config/m68k/lb1sf68.S"), "-o", str(object_path)])
        integer_objects.append(str(object_path))
    # mktime uses public div(), whose SDK archive member also defines the
    # compiler helpers and ldiv. Supply the C entry point separately to keep
    # that conflicting alias bundle out of otherwise fully resolved links.
    integer_div = Path(__file__).resolve().parent / "integer-div.c"
    integer_div_object = libgcc / "integer-div.o"
    run([cc, *cpu_flags, "-O2", "-fno-builtin-div", "-c", str(integer_div),
         "-o", str(integer_div_object)])
    integer_objects.append(str(integer_div_object))
    run([environment["LD"], "-r", *integer_objects, "-o", str(prefix / "lib/integer-math.o")])
    shutil.copy2(sdk_include / "fenv.h", prefix / "include/fenv.h")
    for header in runtime_headers.glob("*.h"):
        shutil.copy2(header, prefix / "include" / header.name)
    license_directory = prefix / "share/gcc-runtime"
    license_directory.mkdir(parents=True, exist_ok=True)
    for name in ("COPYING3", "COPYING.RUNTIME"):
        shutil.copy2(source / name, license_directory / name)
    pthread_distribution = prefix / "share/amiga-pthread"
    pthread_distribution.mkdir(parents=True, exist_ok=True)
    for name in (("pthread_key.c", "pthread_cond.c", "pthread_intern.h", "pthread.h", "debug.h", "Makefile.gcc6") if args.profile == "oct29" else ("pthread.c", "pthread.h")):
        shutil.copy2(pthread_source / "pthreads" / name, pthread_distribution / name)
    if sdk_fingerprint(toolchain, cpu_flags)["sha256"] != fingerprint["sha256"]:
        raise SystemExit("SDK contents changed while installing; no new completion manifest written. "
                         "Partial artifacts were retained; use fresh build/prefix directories.")
    (prefix / "manifest.json").write_text(json.dumps({
        "sdk_fingerprint": fingerprint,
        "native_validation": "Not run by this builder. Prior probe evidence applies only to its recorded binary hashes.",
        "runtime_profile": args.profile,
        "pthread_rebuilt_members": [member + ".o" for member in pthread_members],
        "pthread_compile_flags": pthread_flags,
        "gcc_revision": revision,
        "gcc_repository": "https://github.com/AmigaPorts/gcc",
        "pthread_revision": pthread_revision,
        "pthread_repository": "https://github.com/AmigaPorts/aros-stuff",
        "compiler": subprocess.check_output([cc, "--version"], text=True).splitlines()[0],
        "cpu": args.cpu, "cpu_flags": cpu_flags,
        "thread_model": "posix", "shared_ptr_lock_policy": "atomic",
        "libgcc_rebuilt_members": ["emutls.o"],
        "compiler_helper_objects": [*amigaos3_cpu.math_helpers(args.cpu), "integer-math.o"],
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
            **{"lib/" + name: prefix / "lib" / name for name in amigaos3_cpu.math_helpers(args.cpu)},
            "lib/integer-math.o": prefix / "lib/integer-math.o",
            "integer-div.c": integer_div,
            "libgcc/config/m68k/lb1sf68.S": source / "libgcc/config/m68k/lb1sf68.S",
            "libgcc/config/m68k/fpgnulib.c": source / "libgcc/config/m68k/fpgnulib.c",
            "lib/libpthread.a": prefix / "lib/libpthread.a",
            **{member + ".c": pthread_source / "pthreads" / (member + ".c") for member in pthread_members},
            "pthread_patch": pthread_patch,
            "sdk/libpthread.a": sdk_pthread,
        }.items()},
        "limitations": ["C++17 application runtime; GCC C++23 module interfaces are not validated",
                        "Other SDK libgcc members are retained; application C++ exceptions are disabled"]
    }, indent=2) + "\n")
    print("C++ runtime overlay installed:", prefix)


if __name__ == "__main__":
    main()
