#!/usr/bin/env python3
"""Run the OS3 platform test in a disposable copy of an installed system."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("system", "rom", "emulator", "binary", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--mui-library", type=Path)
    parser.add_argument("--script", type=Path, help="JavaScript file to run with a jsc binary")
    parser.add_argument("--data-hdf", type=Path,
                        help="copy and mount a 512-byte FFS image as DH1 (32 sectors, 1 head, 2 reserved)")
    parser.add_argument("--stack", type=int, default=262144)
    parser.add_argument("--z3-memory", type=int, default=64)
    parser.add_argument("--rtg-memory", type=int, default=0,
                        help="enable a Zorro III RTG card (MiB) and load guest monitor drivers")
    parser.add_argument("--bsdsocket", action="store_true",
                        help="enable WinUAE's native socket-library emulation")
    parser.add_argument("--gdb-port", type=int, default=2345)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--startup-delay", type=int, default=0,
                        help="guest seconds to allow arming a GDB process breakpoint")
    parser.add_argument("--success-marker", default="platform test: 0 failures")
    args = parser.parse_args()
    if args.stack < 4096 or args.z3_memory not in (16, 32, 64, 128, 256, 512):
        parser.error("invalid guest stack size or Zorro III memory size")
    if args.rtg_memory not in (0, 4, 8, 16, 32, 64):
        parser.error("invalid RTG memory size")
    if args.rtg_memory and not (args.system / "C/LoadMonDrvs").is_file():
        parser.error("RTG test requires C/LoadMonDrvs in the guest fixture")
    if args.script and not args.script.is_file():
        parser.error("missing JavaScript input: " + str(args.script))
    if args.startup_delay < 0:
        parser.error("startup delay cannot be negative")
    if args.data_hdf and not args.data_hdf.is_file():
        parser.error("missing data hardfile: " + str(args.data_hdf))
    root = args.output.resolve()
    source = args.system.resolve()
    if root == source or source in root.parents or root in source.parents:
        parser.error("output and source system must be separate directory trees")
    for required in (source / "C/Assign", source / "C/MakeDir", args.rom,
                     args.emulator, args.binary):
        if not required.is_file():
            parser.error("missing input: " + str(required))
    # Refuse to compete with an existing debugger or emulator.
    with socket.socket() as port_check:
        port_check.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        port_check.bind(("127.0.0.1", args.gdb_port))
    root.mkdir(parents=True, exist_ok=False)
    guest = root / "system"
    shutil.copytree(source, guest)
    shutil.copy2(args.binary, guest / "odyssey-platform-test")
    arguments = "--mui"
    if args.script:
        shutil.copy2(args.script, guest / "odyssey-test.js")
        arguments = "SYS:odyssey-test.js"
    if args.mui_library:
        shutil.copy2(args.mui_library, guest / "Libs/muimaster.library")
    for item in guest.rglob("*"):
        if item.is_file():
            item.chmod(item.stat().st_mode | 0o100)
    startup = guest / "S/Startup-Sequence"
    extra_assigns = "C:Assign AmiSSL: SYS:AmiSSL\n" if (guest / "AmiSSL").is_dir() else ""
    delay = f"C:Wait {args.startup_delay} SEC\n" if args.startup_delay else ""
    if args.startup_delay and not (guest / "C/Wait").is_file():
        parser.error("startup delay requires C/Wait in the guest fixture")
    monitor_setup = "C:LoadMonDrvs\n" if args.rtg_memory else ""
    startup.write_text(
        "C:Assign ENV: SYS:Prefs/Env-Archive\nFailAt 21\n"
        "C:MakeDir RAM:T RAM:ENV\nC:Assign T: RAM:T\n"
        "C:Assign ENV: RAM:ENV\nC:Assign MUI: SYS:\n"
        "C:Assign LIBS: SYS:Classes ADD\n" + extra_assigns + monitor_setup + delay + f"Stack {args.stack}\n"
        f"SYS:odyssey-platform-test {arguments} >SYS:odyssey-result.txt\n"
        'Echo "RETURN=$RC" >>SYS:odyssey-result.txt\n'
        "Echo DONE >>SYS:odyssey-result.txt\n")
    result = guest / "odyssey-result.txt"
    result.unlink(missing_ok=True)
    config = root / "run.uae"
    data_disk = ""
    if args.data_hdf:
        data_path = root / "data.hdf"
        shutil.copy2(args.data_hdf, data_path)
        data_disk = f"hardfile2=rw,DH1:{data_path},32,1,2,512,-128,,\n"
    rtg_config = (f"gfxcard_size={args.rtg_memory}\ngfxcard_type=ZorroIII\n"
                  if args.rtg_memory else "")
    config.write_text(
        "config_description=Odyssey OS3 platform test\nuse_gui=no\n"
        f"kickstart_rom_file={args.rom.resolve()}\n"
        # WinUAE's default FPU compares by subtraction, so equal infinities
        # differ, and ignores the FPCR rounding precision. fpu_strict fixes
        # both and keeps the fast host FPU.
        "cpu_model=68020\nfpu_model=68882\nfpu_strict=true\n"
        "cpu_24bit_addressing=false\ncpu_speed=max\n"
        "cpu_compatible=false\nchipset=aga\nchipmem_size=4\n"
        f"z3mem_size={args.z3_memory}\nfastmem_size=8\nsound_output=none\n"
        f"bsdsocket_emu={'true' if args.bsdsocket else 'false'}\n"
        "gfx_width_windowed=640\ngfx_height_windowed=480\n"
        f"filesystem2=rw,DH0:System:{guest},0\n"
        f"uaehf0=dir,rw,DH0:System:{guest},0\n"
        + data_disk + rtg_config +
        f"debugging_features=gdbserver\ngdb_port={args.gdb_port}\n")
    inputs = {name: {"path": str(path.resolve()), "sha256": digest(path)}
              for name, path in (("binary", args.binary), ("rom", args.rom),
                                 ("emulator", args.emulator), ("config", config))}
    if args.data_hdf:
        inputs["data_hdf"] = {"path": str(args.data_hdf.resolve()), "sha256": digest(args.data_hdf)}
    inputs["guest_files"] = {
        str(path.relative_to(guest)): digest(path)
        for path in sorted(guest.rglob("*")) if path.is_file()}
    (root / "inputs.json").write_text(json.dumps(inputs, indent=2) + "\n")
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen",
               SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    text = ""
    with (root / "emulator.log").open("w") as log:
        process = subprocess.Popen([str(args.emulator.resolve()), "-f", str(config)],
                                   env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + args.timeout
            while time.monotonic() < deadline:
                if result.exists():
                    text = result.read_text(errors="replace")
                    if "DONE" in text:
                        break
                if process.poll() is not None:
                    raise RuntimeError("emulator exited before test completion")
                time.sleep(1)
            else:
                raise RuntimeError("guest test timed out; inspect emulator.log/GDB")
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            if result.exists():
                shutil.copyfile(result, root / "output.txt")
    print(text, end="")
    if args.success_marker not in text or "RETURN=0" not in text:
        raise RuntimeError("guest platform checks failed")


if __name__ == "__main__":
    main()
