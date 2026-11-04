#!/usr/bin/env python3
"""Extract original particle initialization, forces and contact-response instructions for optional x86 parity.

This reference is independent of native CMake builds. Requires i686 Windows GCC
and Windows/Wine to regenerate fixture output; does not require Zig.
"""
import argparse
import ast
from pathlib import Path
import shutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
args.output.mkdir(parents=True, exist_ok=True)
if any((args.output / name).exists() for name in ["particles.s", "cloth_particles_x86.cpp"]):
    parser.error("reference output files already exist")
assembly = ".intel_syntax noprefix\n.text\n"
for file, symbol in [("network/session_helper_instructions.cpp", "00580e30"),
                     ("network/session_helper_instructions.cpp", "00580da0"),
                     ("network/session_helper_instructions.cpp", "00581190"),
                     ("startup/startup_dependency_instructions.cpp", "00581770"),
                     ("startup/startup_dependency_instructions.cpp", "00581890"),
                     ("startup/startup_dependency_instructions.cpp", "00582540"),
                     ("startup/startup_dependency_instructions.cpp", "00582d90")]:
    source = (root / "src/client" / file).read_text()
    start = source.index('"_FUN_' + symbol + ':')
    end = source.index('".globl _FUN_', start + 1)
    assembly += ".globl _FUN_" + symbol + "\n"
    assembly += "".join(ast.literal_eval(line) for line in source[start:end].splitlines()
                        if line.startswith('"'))
source = (root / "src/client/startup/startup_dependency_instructions.cpp").read_text()
# Wrap the unmodified correction-loop bodies with scratch space and preserved
# registers. The original routines' force/integration setup is outside this probe.
for name, start_label, end_label, exit_label, counter in [
    ("cloth_correct_scaled_original", "005845b0", "005846a4", "005846c7", "ebp"),
    ("cloth_correct_unscaled_original", "00584853", "00584934", "00584934", "ebx"),
]:
    start = source.index('"_startup_dependency_location_' + start_label + ':')
    end = source.index('"_startup_dependency_location_' + end_label + ':', start)
    body = "".join(ast.literal_eval(line) for line in source[start:end].splitlines()
                   if line.startswith('"'))
    assembly += (".globl _" + name + "\n_" + name + ":\n"
                 "push ebx\npush ebp\npush esi\npush edi\nsub esp,0x40\n"
                 "mov esi,ecx\nxor edx,edx\nxor " + counter + "," + counter + "\n"
                 "cmp dword ptr [esi+0x2c],0\njle _" + name + "_done\n" + body)
    # Scaled loop exits at 005846c7; its lexical end is the later near-contact branch.
    assembly += ("_startup_dependency_location_" + exit_label + ":\n_" + name + "_done:\n"
                 "add esp,0x40\npop edi\npop esi\npop ebp\npop ebx\nret\n")
(args.output / "particles.s").write_text(assembly)
shutil.copyfile(root / "tests/reference/cloth_particles_x86.cpp", args.output / "cloth_particles_x86.cpp")
print(args.output)
