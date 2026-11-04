#!/usr/bin/env python3
"""Extract original cloth detector for independent x86 body-shape and contact references."""
import argparse
import ast
from pathlib import Path
import shutil
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
a.output.mkdir(parents=True,exist_ok=True)
if any((a.output/name).exists() for name in ['detector.s','cloth_body_x86.cpp']):
    p.error('reference output files already exist')
s=(root/'src/client/startup/startup_dependency_instructions.cpp').read_text()
start=s.index('"_FUN_00583370:')
end=s.index('".globl _FUN_',start+1)
assembly=''.join(ast.literal_eval(l) for l in s[start:end].splitlines() if l.startswith('"'))
(a.output/'detector.s').write_text('.intel_syntax noprefix\n.text\n.globl _FUN_00583370\n'+assembly)
shutil.copyfile(root/'tests/reference/cloth_body_x86.cpp',a.output/'cloth_body_x86.cpp')
print(a.output)
