#!/usr/bin/env python3
"""Extract full original cloth solvers/frame policy for optional x86 parity."""
import argparse
import ast
from pathlib import Path
import shutil
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
a=p.parse_args();root=Path(__file__).resolve().parents[1]
a.output.mkdir(parents=True,exist_ok=True)
if any((a.output/name).exists() for name in ['simulation.s','cloth_simulation_x86.cpp']):
    p.error('reference output files already exist')
assembly='.intel_syntax noprefix\n.text\n'
for path,symbols in [
    ('network/session_helper_instructions.cpp',['00580e30','00580da0','00581190']),
    ('startup/startup_dependency_instructions.cpp',['00581770','00581890','005819d0','00582540',
      '00582d90','00583370','005843e0','005847c0','005849a0','00584ba0'])]:
    source=(root/'src/client'/path).read_text()
    for symbol in symbols:
        start=source.index('"_FUN_'+symbol+':');end=source.index('".globl _FUN_',start+1)
        body=''.join(ast.literal_eval(l) for l in source[start:end].splitlines() if l.startswith('"'))
        if symbol=='00584ba0':
            # Transparent counters observe parent-level calls; solver bodies
            # remain original. The observer changes no registers or FP state.
            for target in ['005843e0','005849a0','005819d0']:
                body=body.replace('call _FUN_'+target+'\n','call _trace_'+target+'\n')
            for label in ['00584bf7','00584c92']:
                body=body.replace('_startup_dependency_location_'+label+':\n',
                    '_startup_dependency_location_'+label+':\ninc dword ptr [_trace_substeps]\n')
        assembly+='.globl _FUN_'+symbol+'\n'+body
for target,counter in [('005843e0','attempts'),('005849a0','attempts'),('005819d0','resets')]:
    assembly+='.globl _trace_'+target+'\n_trace_'+target+':\ninc dword ptr [_trace_'+counter+']\njmp _FUN_'+target+'\n'
(a.output/'simulation.s').write_text(assembly)
shutil.copyfile(root/'tests/reference/cloth_simulation_x86.cpp',a.output/'cloth_simulation_x86.cpp')
print(a.output)
