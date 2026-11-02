"""Build-time native expression emitter shared by validated CPU-specific exporters."""

def write_changed(path,contents):
    if not path.exists() or path.read_text()!=contents:path.write_text(contents)

supported={'COPY','LOAD','STORE','INT_ADD','INT_SUB','INT_MULT','INT_AND','INT_OR','INT_XOR','INT_NEGATE','INT_2COMP',
           'INT_EQUAL','INT_NOTEQUAL','INT_LESS','INT_LESSEQUAL','INT_SLESS','INT_SLESSEQUAL','INT_LEFT','INT_RIGHT',
           'INT_SRIGHT','INT_ZEXT','INT_SEXT','SUBPIECE','PIECE','BOOL_NEGATE','BOOL_OR','BOOL_AND','BOOL_XOR',
           'BRANCH','CBRANCH','BRANCHIND','CALL','CALLIND','RETURN',
           'INT_CARRY','INT_SCARRY','INT_SBORROW','INT_DIV','INT_REM','INT_SDIV','INT_SREM'}

def emit_page(page,items,outdir,unsupported,namespace="aot",header="recompiled.hpp"):
    available={row['address'] for row in items}
    code=['// Generated at build time from verified source bytes; do not edit.', f'#include "{header}"',
          f'namespace tecmo98::{namespace} {{', f'void page_{page:x}(Context& c,DataBus& data) {{', 'switch(c.pc) {']
    for address in sorted(available):code.append(f'case 0x{address:x}:goto L_{address:x};')
    code+=['default:throw std::runtime_error("Untranslated native entry: "+std::to_string(c.pc));','}']
    def transfer(address):
        return f'c.pc=0x{address:x};'+(f'goto L_{address:x};' if address in available else 'return;')
    for row in sorted(items,key=lambda row:row['address']):
        address=row['address'];ops=row['ops']
        code.append(f'L_{address:x}: {{ // {row["asm"]}')
        code.append(f'c.pc=0x{address:x};if(c.pc==c.stop||c.remaining==0||c.yieldRequested)return;')
        value=int(row['bytes'],16);width=len(row['bytes'])//2
        if width in (2,4):
            code.append(f'data.verifySource(0x{address:x},0x{value:x}u,{width});--c.remaining;++c.executed;')
        else:
            assert width>0 and width<=32
            original=bytes.fromhex(row['bytes'])
            for offset in range(0,width,2):
                piece=original[offset:offset+2]
                code.append(f'data.verifySource(0x{address+offset:x},0x{int.from_bytes(piece,"big"):x}u,{len(piece)});')
            code.append('--c.remaining;++c.executed;')
        if row.get('native_special')=='68000_ori_ccr':
            original=bytes.fromhex(row['bytes']);assert len(original)==4 and original[:2]==bytes.fromhex('003c')
            code.append(f'orCcr(c,0x{original[3]:x});'+transfer(row['fall'])+'}')
            continue
        unknown=[op[0] for op in ops if op[0] not in supported]
        if any(output and output[0] not in ('unique','register','ram') for _,output,_ in ops):unknown.append('INVALID_OUTPUT_SPACE')
        if any(node[2]>8 for _,output,inputs in ops for node in ([output] if output else [])+inputs):unknown.append('WIDE_VALUE')
        if unknown:
            unsupported.append({'address':address,'assembly':row['asm'],'operations':unknown})
            code.append(f'throw std::runtime_error("Untranslated source operation at 0x{address:x}: {unknown[0]}");'+'}')
            continue
        positions=sorted({byte for _,output,inputs in ops for node in ([output] if output else [])+inputs
                          if node[0]=='unique' for byte in range(node[1],node[1]+node[2])})
        dense={position:index for index,position in enumerate(positions)}
        if dense:code.append(f'std::array<std::uint8_t,{len(dense)}> t{{}};')
        def read(node):
            space,offset,size=node
            assert 1<=size<=8
            if space=='const':return f'0x{offset:x}ull'
            if space=='unique':return f'be<{size}>(t,{dense[offset]})'
            if space=='register':return f'be<{size}>(c.abi,{offset})'
            if space=='ram':return f'data.read(0x{offset:x},{size})'
            raise ValueError(node)
        def assign(node,expression):
            space,offset,size=node
            if space in ('register','unique'):
                return f'setBe<{size}>({"c.abi" if space=="register" else "t"},{offset if space=="register" else dense[offset]},{expression});'
            if space=='ram':return f'data.write(0x{offset:x},{size},{expression});'
            raise ValueError(node)
        local_targets=set()
        for index,(op,output,inputs) in enumerate(ops):
            if op in ('BRANCH','CBRANCH') and inputs[0][0]=='const':
                offset=inputs[0][1];bits=inputs[0][2]*8
                if offset&(1<<(bits-1)):offset-=1<<bits
                local_targets.add(index+offset)
        repair_indices=set();repair_starts=set()
        if row.get('native_repair')=='68000_status_pack':
            start=None
            for index,(op,output,inputs) in enumerate(ops):
                if op=='INT_LEFT' and inputs[0]==['register',64,1] and inputs[1][1]==15:start=index
                if start is not None and op=='INT_ZEXT' and output==['register',512,2]:
                    repair_starts.add(start);repair_indices.update(range(start,index+1));start=None
            assert repair_starts and not repair_indices.intersection(local_targets)
        for index,(op,output,inputs) in enumerate(ops):
            if index in repair_indices:
                if index in repair_starts:code.append('setBe<2>(c.abi,512,packStatus(c));')
                continue
            if index in local_targets:code.append(f'O_{address:x}_{index}:;')
            args=[read(node) for node in inputs] if op not in ('BRANCH','CBRANCH','CALL') else []
            expression=None
            binary={'INT_ADD':'+','INT_SUB':'-','INT_MULT':'*','INT_AND':'&','INT_OR':'|','INT_XOR':'^',
                    'INT_EQUAL':'==','INT_NOTEQUAL':'!=','INT_LESS':'<','INT_LESSEQUAL':'<='}
            if op in binary:expression=f'({args[0]}{binary[op]}{args[1]})'
            elif op in ('COPY','INT_ZEXT'):expression=args[0]
            elif op=='INT_SEXT':expression=f'std::uint64_t(sign({args[0]},{inputs[0][2]}))'
            elif op=='INT_NEGATE':expression=f'(~{args[0]})'
            elif op=='INT_2COMP':expression=f'(0ull-{args[0]})'
            elif op in ('INT_CARRY','INT_SCARRY','INT_SBORROW'):
                helper={'INT_CARRY':'carry','INT_SCARRY':'signedCarry','INT_SBORROW':'signedBorrow'}[op]
                expression=f'{helper}({args[0]},{args[1]},{inputs[0][2]})'
            elif op in ('INT_DIV','INT_REM','INT_SDIV','INT_SREM'):
                expression=f'divide({args[0]},{args[1]},{inputs[0][2]},{str(op in ("INT_SDIV","INT_SREM")).lower()},{str(op in ("INT_REM","INT_SREM")).lower()})'
            elif op=='BOOL_NEGATE':expression=f'({args[0]}==0)'
            elif op in ('BOOL_OR','BOOL_AND','BOOL_XOR'):
                operator={'BOOL_OR':'||','BOOL_AND':'&&','BOOL_XOR':'!='}[op]
                expression=f'(({args[0]}!=0){operator}({args[1]}!=0))'
            elif op in ('INT_SLESS','INT_SLESSEQUAL'):
                expression=f'(sign({args[0]},{inputs[0][2]}){"<" if op=="INT_SLESS" else "<="}sign({args[1]},{inputs[1][2]}))'
            elif op in ('INT_LEFT','INT_RIGHT','INT_SRIGHT'):
                expression=f'{dict(INT_LEFT="left",INT_RIGHT="right",INT_SRIGHT="arithmetic")[op]}({args[0]},{args[1]},{inputs[0][2]})'
            elif op=='SUBPIECE':expression=f'right({args[0]},{args[1]}*8,{inputs[0][2]})'
            elif op=='PIECE':expression=f'(({args[0]}<<{inputs[1][2]*8})|{args[1]})'
            elif op=='LOAD':
                assert inputs[0][0]=='const' and inputs[0][1]==0x1a1
                expression=f'data.read(std::uint32_t({args[1]}),{output[2]})'
            elif op=='STORE':
                assert inputs[0][0]=='const' and inputs[0][1]==0x1a1
                code.append(f'data.write(std::uint32_t({args[1]}),{inputs[2][2]},{args[2]});')
            elif op in ('BRANCH','CBRANCH','CALL'):
                target=inputs[0]
                if target[0]=='const':
                    offset=target[1];bits=target[2]*8
                    if offset&(1<<(bits-1)):offset-=1<<bits
                    jump=f'goto O_{address:x}_{index+offset};'
                else:
                    assert target[0]=='ram';jump=transfer(target[1])
                code.append((f'if({read(inputs[1])}!=0){{{jump}}}' if op=='CBRANCH' else jump))
            elif op in ('CALLIND','BRANCHIND','RETURN'):
                code.append(f'c.pc=std::uint32_t({args[0]});return;')
            else:raise ValueError(op)
            if expression is not None:code.append(assign(output,expression))
        if len(ops) in local_targets:code.append(f'O_{address:x}_{len(ops)}:;')
        if row['fall'] is not None:code.append(transfer(row['fall']))
        else:code.append('return;')
        code.append('}')
    code+=['}','}']
    path=outdir/f'page_{page:x}.cpp';write_changed(path,'\n'.join(code)+'\n')
    return path

