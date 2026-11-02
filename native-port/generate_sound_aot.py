"""Static native compilation of the recovered 68000 sound driver."""
import collections
import hashlib
import json
import pathlib
from aot_emitter import emit_page,write_changed

root=pathlib.Path(__file__).resolve().parent
ir=root.parent/'work/sound-blocks.jsonl'
rows=[json.loads(line) for line in ir.read_text().splitlines()]
metadata=rows.pop(0)['metadata'];original=(root.parent/'work/sound-driver.bin').read_bytes()
assert hashlib.sha256(original).hexdigest()==metadata['driver_sha256']=='5e7f7b6b2275afaffe351267fbd382a949b12250a6feabfb36f53b030b2d3000'
assert metadata['language']=='68000:BE:32:default'
for row in rows:
    code=bytes.fromhex(row['bytes']);assert original[row['address']:row['address']+len(code)]==code
    for _,output,inputs in row['ops']:
        for node in ([output] if output else [])+inputs:
            if node[0]=='register':assert node[1]+node[2]<=metadata['abi_size']
outdir=root/'generated-sound';outdir.mkdir(exist_ok=True);pages=collections.defaultdict(list)
for row in rows:pages[row['address']>>8].append(row)
unsupported=[];paths=[emit_page(page,items,outdir,unsupported,'aot_sound','recompiled_sound.hpp') for page,items in sorted(pages.items())]
dispatch=['#include "recompiled_sound.hpp"','namespace tecmo98::aot_sound {']
for page in sorted(pages):dispatch.append(f'void page_{page:x}(Context&,DataBus&);')
dispatch+=['void run(Context& c,DataBus& data,std::uint64_t budget){c.remaining=budget;while(c.remaining!=0&&c.pc!=c.stop&&!c.yieldRequested){switch(c.pc>>8){']
for page in sorted(pages):dispatch.append(f'case 0x{page:x}:page_{page:x}(c,data);break;')
dispatch+=['default:throw std::runtime_error("Untranslated native sound page: "+std::to_string(c.pc));','}}}','}']
write_changed(outdir/'dispatch.cpp','\n'.join(dispatch)+'\n');paths.append(outdir/'dispatch.cpp')
write_changed(outdir/'sources.cmake','set(TECMO98_SOUND_SOURCES\n'+''.join(f'  "${{CMAKE_CURRENT_LIST_DIR}}/{path.name}"\n' for path in paths)+')\n')
report={'instructions':len(rows),'pages':len(pages),'unsupported':unsupported,'native_overrides':[{'address':row['address'],'operation':row.get('native_special',row.get('native_repair'))} for row in rows if row.get('native_special') or row.get('native_repair')],'source':metadata,'ir_sha256':hashlib.sha256(ir.read_bytes()).hexdigest(),
        'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'emitter_sha256':hashlib.sha256((root/'aot_emitter.py').read_bytes()).hexdigest()}
(outdir/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'instructions':len(rows),'pages':len(pages),'explicit_stops':len(unsupported)}))
