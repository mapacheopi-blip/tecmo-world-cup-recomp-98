"""Build-time lowering of fixed, provenance-checked Ghidra raw p-code into native C++.

The output has C++ expressions/gotos, not an opcode/IR interpreter. Unknown operations
produce an explicit stop at that source address, never a successful empty translation.
"""
import collections
import hashlib
import json
import pathlib
import sys

root=pathlib.Path(__file__).resolve().parent
source=root.parent/'work/native-blocks.jsonl'
rows=[json.loads(line) for line in source.read_text().splitlines()]
metadata=rows.pop(0)['metadata']
assert metadata['language']=='SuperH:BE:32:SH-2'
assert metadata['eprom_sha1']=='f66cc955181720661a0334fe67fa5750ddf9758b'
assert metadata['bios_sha256']=='9163bd0f1727b390c9859dc033454f1ffd56528578d89f27381a3b9421bd8fd1'
eprom=(root.parent/'work/boot-epr.bin').read_bytes();bios=(root.parent/'work/bios-jp1-be.bin').read_bytes()
profile=(root.parent/'firmware/resident-jp1.bin').read_bytes()
assert hashlib.sha1(eprom).hexdigest()==metadata['eprom_sha1']
assert hashlib.sha256(bios).hexdigest()==metadata['bios_sha256']
assert hashlib.sha256(profile).hexdigest()==metadata['profile_sha256']
for row in rows:
    at=row['address'];at=at&~0x20000000 if at<0x40000000 else at;expected=bytes.fromhex(row['bytes'])
    original=bios[at:at+len(expected)] if at<0x80000 else (profile if at<0x6004000 else eprom)[at-0x6000000:at-0x6000000+len(expected)]
    assert original==expected,hex(at)

# Ghidra 12.1.4 superh.sinc MAC.L: `if (Res2 >= temp1)` uses
# Res2=0 instead of the sum Res1. Hitachi h12p0 MAC.L specifies
# `if (Res1 < temp1) Res2 += 0x10000`. Preserve the raw IR and
# explicitly record the corrected comparison, including delay slots.
repairs=[]
for row in rows:
    ops=row['ops']
    for i,(op,output,inputs) in enumerate(ops):
        if i<2 or op!='INT_LESSEQUAL':continue
        zero,addition=ops[i-2],ops[i-1]
        if zero[0]=='COPY' and zero[2]==[['const',0,4]] and addition[0]=='INT_ADD' and inputs==[addition[2][0],zero[1]] and ops[i+1][0]=='CBRANCH' and ops[i+2][0]=='INT_ADD' and ops[i+2][2]==[zero[1],['const',65536,4]]:
            inputs[1]=addition[1]
            repairs.append({'address':row['address'],'operation':'SH2_MACL_cross_product_carry','pcode_index':i})
            # In S=1 mode MACH[15:0] is the signed upper half of the
            # 48-bit accumulator, not an unsigned positive addend.
            for entry in ops[i+1:]:
                if entry[0]=='INT_AND' and entry[2]==[['register',268,4],['const',65535,4]]:
                    entry[0]='INT_SEXT';entry[2]=[['register',270,2]]
                    repairs.append({'address':row['address'],'operation':'SH2_MACL_signed_48bit_accumulator'})

from aot_architecture import repair_self_predecrement
for row in rows:repairs.extend(repair_self_predecrement(row))

outdir=root/'generated';outdir.mkdir(exist_ok=True)
pages=collections.defaultdict(list)
for row in rows:pages[row['address']>>8].append(row)
unsupported=[]
def write_changed(path,contents):
    if not path.exists() or path.read_text()!=contents:path.write_text(contents)
from aot_emitter import emit_page

paths=[emit_page(page,items,outdir,unsupported) for page,items in sorted(pages.items())]
dispatch=['#include "recompiled.hpp"','namespace tecmo98::aot {']
for page in sorted(pages):dispatch.append(f'void page_{page:x}(Context&,DataBus&);')
dispatch+=['void run(Context& c,DataBus& data,std::uint64_t budget){c.remaining=budget;',
           'while(c.remaining!=0&&c.pc!=c.stop&&!c.yieldRequested){if(data.runNativeFunction(c))continue;switch(c.pc>>8){']
for page in sorted(pages):dispatch.append(f'case 0x{page:x}:page_{page:x}(c,data);break;')
dispatch+=['default:throw std::runtime_error("Untranslated native page: "+std::to_string(c.pc));','}}}','}']
write_changed(outdir/'dispatch.cpp','\n'.join(dispatch)+'\n');paths.append(outdir/'dispatch.cpp')
write_changed(outdir/'sources.cmake','set(TECMO98_GENERATED_SOURCES\n'+''.join(f'  "${{CMAKE_CURRENT_LIST_DIR}}/{path.name}"\n' for path in paths)+')\n')
report={'instructions':len(rows),'pages':len(pages),'ir_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
        'emitter_sha256':hashlib.sha256((root/'aot_emitter.py').read_bytes()).hexdigest(),
        'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
        'architecture_sha256':hashlib.sha256((root/'aot_architecture.py').read_bytes()).hexdigest(),
        'unsupported':unsupported,'architectural_repairs':repairs,'source':metadata,'runtime':'Static C++ expressions and labels; no IR evaluator, opcode decoder or JIT'}
(outdir/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({key:report[key] for key in ('instructions','pages','unsupported')}))
