#!/usr/bin/env python3
"""Extract original x86 routines for an optional migration parity reference.

This generator does not build the native client and introduces no runtime
dependency. Compile its output with an i686 Windows C++ compiler, then run the
result on Windows or Wine. Output files must not already exist.
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
names = ["animation_pose_x86.cpp", "pose.s", "matrix.cpp", "quaternion.cpp", "from_matrix.cpp"]
if any((args.output / name).exists() for name in names):
    parser.error("reference output files already exist")
source = (root / "src/client/startup/startup_dependency_instructions.cpp").read_text()
start = source.index('"_FUN_00552910:')
end = source.index('".globl _FUN_00552cb0', start)
assembly = "".join(ast.literal_eval(line) for line in source[start:end].splitlines()
                   if line.startswith('"'))
start = source.index('"_FUN_00552cb0:')
end = source.index('".globl _FUN_', start + 1)
assembly += ".globl _FUN_00552cb0\n" + "".join(ast.literal_eval(line) for line in source[start:end].splitlines()
                                                if line.startswith('"'))
(args.output / "pose.s").write_text(".intel_syntax noprefix\n.text\n.globl _FUN_00552910\n" + assembly)
for output, source_name, symbol, stop in [
    ("matrix.cpp", "render_matrix_rotations.cpp", "recovered_005e5adf", "FUN_005e5adf"),
    ("quaternion.cpp", "render_quaternion_interpolation.cpp", "recovered_005e6d56", "FUN_005e6d56"),
    ("from_matrix.cpp", "render_quaternion_from_matrix.cpp", "recovered_005e6789", "FUN_005e6789"),
]:
    source = (root / "src/client/graphics" / source_name).read_text()
    prelude = source[source.index("namespace {"):source.index("// Complete original")]
    start = source.index('extern "C" void* ' + symbol)
    end = source.index('extern "C" __attribute__((naked, stdcall)) void* ' + stop, start)
    (args.output / output).write_text("#include <stdint.h>\n" + prelude + source[start:end])
shutil.copyfile(root / "tests/reference/animation_pose_x86.cpp", args.output / names[0])
print(args.output)
