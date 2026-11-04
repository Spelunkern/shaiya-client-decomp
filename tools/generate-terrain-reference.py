#!/usr/bin/env python3
"""Extract original terrain plane interpolation and sample lookup for optional x86 verification."""
import argparse
import ast
from pathlib import Path
import shutil
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('output', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
a.output.mkdir(parents=True, exist_ok=True)
if any((a.output / n).exists() for n in ['terrain.s', 'terrain_x86.cpp']):
    p.error('reference output files already exist')
s = (root / 'src/client/network/session_helper_instructions.cpp').read_text()
assembly = '.intel_syntax noprefix\n.text\n'
for symbol in ['005b3640', '005b3360', '005b3720', '005a2c30']:
    start = s.index('"_FUN_' + symbol + ':')
    end = s.index('".globl _FUN_', start + 1)
    assembly += '.globl _FUN_' + symbol + '\n'
    assembly += ''.join(ast.literal_eval(l) for l in s[start:end].splitlines() if l.startswith('"'))
(a.output / 'terrain.s').write_text(assembly)
shutil.copyfile(root / 'tests/reference/terrain_x86.cpp', a.output / 'terrain_x86.cpp')
print(a.output)
