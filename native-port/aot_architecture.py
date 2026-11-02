"""Build-time SH-2 architectural corrections, separate from the raw Ghidra IR."""
def repair_self_predecrement(row):
    raw=bytes.fromhex(row['bytes']);sites=[]
    for offset in range(0,len(raw),2):
        word=int.from_bytes(raw[offset:offset+2],'big')
        if word&0xf00f in (0x2004,0x2005,0x2006) and (word>>4&15)==(word>>8&15):
            sites.append((offset,word))
    if not sites:return []
    if len(sites)!=1:raise ValueError('Ambiguous self-predecrement instruction')
    offset,word=sites[0];size=1<<((word&15)-4);register=(word>>8&15)*4
    full=['register',register,4];source=['register',register+4-size,size];ops=row['ops']
    pairs=[i for i in range(len(ops)-1) if ops[i]==['INT_SUB',full,[full,['const',size,4]]]
           and ops[i+1][0]=='STORE' and ops[i+1][2][1:]==[full,source]]
    if len(pairs)!=1:raise ValueError(f"Unexpected Ghidra predecrement form at {row['address']:08x}")
    maximum=max([0]+[v[1]+v[2] for op,out,inputs in ops for v in ([out] if out else [])+inputs if v[0]=='unique'])
    saved=['unique',(maximum+15)&~15,size];at=pairs[0]
    # Preserve Rm before updating Rn, including m==n and delay-slot instances.
    ops[at+1][2][2]=saved;ops.insert(at,['COPY',saved,[source]])
    return [{'address':row['address'],'instruction_address':row['address']+offset,
             'operation':'SH2_MOV_self_predecrement_original_value','size':size,'register':register//4,'delay_slot':offset!=0}]
