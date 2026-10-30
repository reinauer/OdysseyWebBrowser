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
    parser.add_argument("--gdb-port", type=int, default=2345)
    parser.add_argument("--timeout", type=int, default=120)
    args = parser.parse_args()
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
        port_check.bind(("127.0.0.1", args.gdb_port))
    root.mkdir(parents=True, exist_ok=False)
    guest = root / "system"
    shutil.copytree(source, guest)
    shutil.copy2(args.binary, guest / "odyssey-platform-test")
    if args.mui_library:
        shutil.copy2(args.mui_library, guest / "Libs/muimaster.library")
    for item in guest.rglob("*"):
        if item.is_file():
            item.chmod(item.stat().st_mode | 0o100)
    startup = guest / "S/Startup-Sequence"
    startup.write_text(
        "C:Assign ENV: SYS:Prefs/Env-Archive\nFailAt 21\n"
        "C:MakeDir RAM:T RAM:ENV\nC:Assign T: RAM:T\n"
        "C:Assign ENV: RAM:ENV\nC:Assign MUI: SYS:\n"
        "C:Assign LIBS: SYS:Classes ADD\nStack 262144\n"
        "SYS:odyssey-platform-test --mui >SYS:odyssey-result.txt\n"
        'Echo "RETURN=$RC" >>SYS:odyssey-result.txt\n'
        "Echo DONE >>SYS:odyssey-result.txt\n")
    result = guest / "odyssey-result.txt"
    result.unlink(missing_ok=True)
    config = root / "run.uae"
    config.write_text(
        "config_description=Odyssey OS3 platform test\nuse_gui=no\n"
        f"kickstart_rom_file={args.rom.resolve()}\n"
        "cpu_model=68020\ncpu_24bit_addressing=false\ncpu_speed=max\n"
        "cpu_compatible=false\nchipset=aga\nchipmem_size=4\n"
        "z3mem_size=64\nfastmem_size=8\nsound_output=none\n"
        "gfx_width_windowed=640\ngfx_height_windowed=480\n"
        f"filesystem2=rw,DH0:System:{guest},0\n"
        f"uaehf0=dir,rw,DH0:System:{guest},0\n"
        f"debugging_features=gdbserver\ngdb_port={args.gdb_port}\n")
    inputs = {name: {"path": str(path.resolve()), "sha256": digest(path)}
              for name, path in (("binary", args.binary), ("rom", args.rom),
                                 ("emulator", args.emulator), ("config", config))}
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
    if "platform test: 0 failures" not in text or "RETURN=0" not in text:
        raise RuntimeError("guest platform checks failed")


if __name__ == "__main__":
    main()
