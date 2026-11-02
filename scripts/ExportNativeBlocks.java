// Build-time static translation input. No p-code or decoder is shipped in the runtime.
import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.program.model.pcode.*;
import ghidra.program.model.lang.UnknownInstructionException;
import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;

public class ExportNativeBlocks extends GhidraScript {
    byte[] eprom,bios,profile;
    String hash(byte[] b,String algo)throws Exception{return HexFormat.of().formatHex(MessageDigest.getInstance(algo).digest(b));}
    String quote(String s){return "\""+s.replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n")+"\"";}
    String node(Varnode v){return v==null?"null":"["+quote(v.getAddress().getAddressSpace().getName())+","+Long.toUnsignedString(v.getOffset())+","+v.getSize()+"]";}
    byte[] source(long a,int size) {
        if(a<0x40000000L)a&=~0x20000000L;
        if(a>=0&&a+size<=bios.length)return Arrays.copyOfRange(bios,(int)a,(int)a+size);
        if(a>=0x06000000L&&a+size<=0x06004000L)return Arrays.copyOfRange(profile,(int)(a-0x06000000L),(int)(a-0x06000000L)+size);
        if(a>=0x06004000L&&a+size<=0x060fc000L)return Arrays.copyOfRange(eprom,(int)(a-0x06000000L),(int)(a-0x06000000L)+size);
        return null;
    }
    Long constant(Varnode v,Map<Long,Long> registers,Map<Long,Long> temps) {
        if(v.isConstant())return v.getOffset();
        if(v.isRegister())return registers.get(v.getOffset());
        if(v.isUnique())return temps.get(v.getOffset());return null;
    }
    long mask(int size){return size==8?-1L:(1L<<(size*8))-1;}
    // Conservative discovery only: product code still performs every load dynamically.
    void discover(PcodeOp op,Map<Long,Long> regs,Map<Long,Long> temps,ArrayDeque<Long> pending) {
        Varnode output=op.getOutput();Long value=null;
        Long a=op.getNumInputs()>0?constant(op.getInput(0),regs,temps):null;
        Long b=op.getNumInputs()>1?constant(op.getInput(1),regs,temps):null;
        switch(op.getOpcode()) {
            case PcodeOp.COPY:case PcodeOp.INT_ZEXT:value=a;break;
            case PcodeOp.INT_SEXT:if(a!=null)value=(a<<(64-op.getInput(0).getSize()*8))>>(64-op.getInput(0).getSize()*8);break;
            case PcodeOp.INT_ADD:if(a!=null&&b!=null)value=a+b;break;
            case PcodeOp.INT_SUB:if(a!=null&&b!=null)value=a-b;break;
            case PcodeOp.INT_AND:if(a!=null&&b!=null)value=a&b;break;
            case PcodeOp.INT_OR:if(a!=null&&b!=null)value=a|b;break;
            case PcodeOp.INT_LEFT:if(a!=null&&b!=null&&b<64)value=a<<b;break;
            case PcodeOp.INT_RIGHT:if(a!=null&&b!=null&&b<64)value=a>>>b;break;
            case PcodeOp.LOAD:
                if(b!=null&&output!=null){byte[] bytes=source(b,output.getSize());if(bytes!=null){value=0L;for(byte n:bytes)value=(value<<8)|(n&255);}}break;
            case PcodeOp.CALLIND:case PcodeOp.BRANCHIND:if(a!=null&&source(a,4)!=null&&(a&1)==0)pending.add(a);break;
        }
        if(output!=null) {
            Map<Long,Long> map=output.isRegister()?regs:output.isUnique()?temps:null;
            if(map!=null){if(value==null)map.remove(output.getOffset());else map.put(output.getOffset(),value&mask(output.getSize()));}
        }
    }
    public void run()throws Exception {
        String[] args=getScriptArgs();Path output=Paths.get(args[0]);
        eprom=Files.readAllBytes(Paths.get(args[1]));bios=Files.readAllBytes(Paths.get(args[2]));profile=Files.readAllBytes(Paths.get(args[3]));
        if(!currentProgram.getLanguageID().toString().equals("SuperH:BE:32:SH-2")||!hash(eprom,"SHA-1").equals("f66cc955181720661a0334fe67fa5750ddf9758b")||!hash(bios,"SHA-256").equals("9163bd0f1727b390c9859dc033454f1ffd56528578d89f27381a3b9421bd8fd1")||!hash(profile,"SHA-256").equals("0420455ca4d92a7c75cac7d8722da401eac7758b49c76345b807002898b883d0"))throw new IOException("Unverified translation inputs");
        currentProgram.getMemory().createInitializedBlock("aot_bios_data",toAddr(0),new ByteArrayInputStream(bios),bios.length,monitor,false);
        currentProgram.getMemory().createInitializedBlock("aot_resident",toAddr(0x06000000L),new ByteArrayInputStream(profile,0,0x4000),0x4000,monitor,false);
        currentProgram.getMemory().createInitializedBlock("aot_program_tail",toAddr(0x06080000L),new ByteArrayInputStream(eprom,0x80000,0x7c000),0x7c000,monitor,false);
        currentProgram.getMemory().createInitializedBlock("aot_bios_alias",toAddr(0x20000000L),new ByteArrayInputStream(bios),bios.length,monitor,false);
        byte[] mirrored=new byte[0xfc000];System.arraycopy(profile,0,mirrored,0,0x4000);System.arraycopy(eprom,0x4000,mirrored,0x4000,0xf8000);
        currentProgram.getMemory().createInitializedBlock("aot_ram_alias",toAddr(0x26000000L),new ByteArrayInputStream(mirrored),mirrored.length,monitor,false);
        byte[] projectCode=new byte[0x7c000];currentProgram.getMemory().getBytes(toAddr(0x06004000L),projectCode);
        if(!Arrays.equals(projectCode,Arrays.copyOfRange(eprom,0x4000,0x80000)))throw new IOException("Project/EPROM mismatch");
        ArrayDeque<Long> pending=new ArrayDeque<>();
        boolean pointerCandidates=false;
        for(int i=4;i<args.length;i++){
            if(args[i].equals("--discover-pointers"))pointerCandidates=true;
            else if(args[i].equals("--discover-all-game-words")){
                // Conservative build-time candidates for computed jumps. This also
                // decodes embedded data; it is not a claim of reachable-code coverage.
                for(long address=0x06004000L;address<0x06065000L;address+=2)pending.add(address);
                for(long address=0x06000c00L;address<0x06004000L;address+=2)pending.add(address);
            }
            else pending.add(Long.parseUnsignedLong(args[i].replace("0x",""),16));
        }
        if(pointerCandidates)for(byte[] image:new byte[][]{eprom,bios,Arrays.copyOf(profile,0x4000)})for(int at=0;at+4<=image.length;at+=4) {
            long value=0;for(int n=0;n<4;n++)value=(value<<8)|(image[at+n]&255);
            long physical=value<0x40000000L?value&~0x20000000L:value;
            if((value&1)==0&&((physical>=0x06004000L&&physical<0x060764d0L)||(physical>=0x06000c00L&&physical<0x06002100L)||(physical>=0x200&&physical<0x4800)))pending.add(value);
        }
        Set<Long> seen=new HashSet<>();Map<String,Integer> operations=new TreeMap<>();int invalid=0,emitted=0;
        var pseudo=new PseudoDisassembler(currentProgram);
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(output))) {
            out.println("{\"metadata\":{\"language\":\"SuperH:BE:32:SH-2\",\"eprom_sha1\":\""+hash(eprom,"SHA-1")+"\",\"bios_sha256\":\""+hash(bios,"SHA-256")+"\",\"profile_sha256\":\""+hash(profile,"SHA-256")+"\"}}");
            while(!pending.isEmpty()) {
                long cursor=pending.remove();Map<Long,Long> registers=new HashMap<>();
                while(!seen.contains(cursor)&&source(cursor,4)!=null&&(cursor&1)==0) {
                    monitor.checkCancelled();seen.add(cursor);
                    ghidra.app.util.PseudoInstruction instruction;
                    try{instruction=pseudo.disassemble(toAddr(cursor));}catch(UnknownInstructionException exception){invalid++;break;}
                    if(instruction==null){invalid++;break;}
                    int delay=instruction.getDelaySlotDepth();byte[] bytes=source(cursor,instruction.getLength()+delay*2);
                    if(bytes==null)throw new IOException("Delay slot outside source");
                    PcodeOp[] ops=instruction.getPcode();Map<Long,Long> temps=new HashMap<>();
                    StringBuilder text=new StringBuilder("{\"address\":"+cursor+",\"bytes\":\""+HexFormat.of().formatHex(bytes)+"\",\"asm\":"+quote(instruction.toString())+",\"fall\":"+(instruction.getFallThrough()==null?"null":instruction.getFallThrough().getOffset())+",\"ops\":[");
                    for(int index=0;index<ops.length;index++) {
                        PcodeOp op=ops[index];operations.merge(op.getMnemonic(),1,Integer::sum);discover(op,registers,temps,pending);
                        if(index!=0)text.append(',');text.append("[").append(quote(op.getMnemonic())).append(',').append(node(op.getOutput())).append(",[");
                        for(int j=0;j<op.getNumInputs();j++){if(j!=0)text.append(',');text.append(node(op.getInput(j)));}text.append("]]");
                    }
                    out.println(text.append("]}"));emitted++;
                    for(var flow:instruction.getFlows())if(source(flow.getOffset(),4)!=null)pending.add(flow.getOffset());
                    if(instruction.getFallThrough()==null)break;
                    cursor=instruction.getFallThrough().getOffset();
                    // Discovery hint only: compiler ABI usually preserves r8-r14. Extra
                    // discovered targets never change dynamically evaluated product calls.
                    if(instruction.getFlowType().isCall())registers.keySet().removeIf(offset->offset<32||offset>=60);
                }
                if(seen.size()>500000)throw new IOException("Unexpected discovery size");
            }
        }
        StringBuilder counts=new StringBuilder("{");for(var entry:operations.entrySet()){if(counts.length()>1)counts.append(',');counts.append(quote(entry.getKey())).append(':').append(entry.getValue());}counts.append('}');
        Files.writeString(Paths.get(output+".json"),"{\"instructions\":"+emitted+",\"attempted\":"+seen.size()+",\"invalid_entries\":"+invalid+",\"candidate_pointer_discovery\":"+pointerCandidates+",\"ir_sha256\":\""+hash(Files.readAllBytes(output),"SHA-256")+"\",\"operations\":"+counts+"}\n");
        println("NATIVE_BLOCK_EXPORT_COMPLETE instructions="+emitted+" operations="+operations+" invalid="+invalid);
    }
}
