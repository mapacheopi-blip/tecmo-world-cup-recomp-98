import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.program.model.pcode.*;
import ghidra.program.model.lang.*;
import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import com.google.gson.*;

public class ExportSoundBlocks extends GhidraScript {
    String hash(byte[] bytes)throws Exception{return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));}
    JsonArray node(Varnode v){if(v==null)return null;JsonArray a=new JsonArray();a.add(v.getAddress().getAddressSpace().getName());a.add(v.getOffset());a.add(v.getSize());return a;}
    public void run()throws Exception{
        String[] args=getScriptArgs();byte[] original=Files.readAllBytes(Paths.get(args[1]));
        if(!hash(original).equals("5e7f7b6b2275afaffe351267fbd382a949b12250a6feabfb36f53b030b2d3000")||!currentProgram.getLanguageID().toString().equals("68000:BE:32:default"))throw new IOException("Unverified sound driver/language");
        byte[] loaded=new byte[original.length];currentProgram.getMemory().getBytes(toAddr(0),loaded);if(!Arrays.equals(loaded,original))throw new IOException("Sound analysis/source mismatch");
        JsonObject metadata=new JsonObject();metadata.addProperty("language",currentProgram.getLanguageID().toString());metadata.addProperty("driver_sha256",hash(original));metadata.addProperty("abi_size",0x800);
        metadata.addProperty("processor_scope","68000 instruction subset of Ghidra 68040-family specification; data candidates are not reachable-code coverage");
        JsonArray registers=new JsonArray();for(Register r:currentProgram.getLanguage().getRegisters())if(r.getBaseRegister()==r&&!r.isProcessorContext()&&r.getMinimumByteSize()<=8&&!r.getName().equals("PC")){
            JsonObject reg=new JsonObject();reg.addProperty("name",r.getName());reg.addProperty("offset",r.getAddress().getOffset());reg.addProperty("size",r.getMinimumByteSize());registers.add(reg);
        }
        metadata.add("registers",registers);JsonObject header=new JsonObject();header.add("metadata",metadata);
        var pseudo=new PseudoDisassembler(currentProgram);int instructions=0,invalid=0;Map<String,Integer> counts=new TreeMap<>();
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(Paths.get(args[0])))){
            out.println(header);
            for(long at=0x1000;at+2<=original.length;at+=2){
                monitor.checkCancelled();ghidra.app.util.PseudoInstruction instruction;
                try{instruction=pseudo.disassemble(toAddr(at));}catch(UnknownInstructionException e){invalid++;continue;}
                if(instruction==null||at+instruction.getLength()>original.length){invalid++;continue;}
                JsonObject row=new JsonObject();row.addProperty("address",at);row.addProperty("bytes",HexFormat.of().formatHex(Arrays.copyOfRange(original,(int)at,(int)at+instruction.getLength())));row.addProperty("asm",instruction.toString());
                if(instruction.getFallThrough()!=null)row.addProperty("fall",instruction.getFallThrough().getOffset());else row.add("fall",JsonNull.INSTANCE);
                JsonArray ops=new JsonArray();for(PcodeOp op:instruction.getPcode()){
                    counts.merge(op.getMnemonic(),1,Integer::sum);JsonArray item=new JsonArray();item.add(op.getMnemonic());item.add(node(op.getOutput()));JsonArray inputs=new JsonArray();for(Varnode v:op.getInputs())inputs.add(node(v));item.add(inputs);ops.add(item);
                }
                if(instruction.toString().equals("rte")){
                    row.add("reference_pcode",ops);row.addProperty("reference_limitation","Ghidra RTE is RETURN(0), not a 68000 exception-frame restoration; requires separate native implementation");
                    JsonArray stop=new JsonArray();stop.add("UNIMPLEMENTED");stop.add(JsonNull.INSTANCE);stop.add(new JsonArray());ops=new JsonArray();ops.add(stop);
                }
                if(instruction.toString().startsWith("ori ")&&instruction.toString().endsWith(",CCR")){
                    row.add("reference_pcode",ops);row.addProperty("reference_limitation","Ghidra's CCR OR unpacks a zero-extended byte into the entire SR, incorrectly changing supervisor/interrupt state; use native CCR-only semantics");
                    row.addProperty("native_special","68000_ori_ccr");
                }
                boolean packedStatus=false;
                for(PcodeOp op:instruction.getPcode())if(op.getOpcode()==PcodeOp.INT_LEFT&&op.getNumInputs()==2&&op.getInput(0).isRegister()&&op.getInput(0).getOffset()==0x40&&op.getInput(1).isConstant()&&op.getInput(1).getOffset()==15&&op.getOutput().getSize()==1)packedStatus=true;
                if(packedStatus&&!row.has("native_special")){
                    row.add("reference_pcode",ops);row.addProperty("reference_limitation","Ghidra packflags shifts one-byte flags by 15/13/8 before zero extension, truncating the SR upper byte; restore the architectural 16-bit packing");
                    row.addProperty("native_repair","68000_status_pack");
                }
                boolean indirect68020=false;for(PcodeOp op:instruction.getPcode())if(op.getOpcode()==PcodeOp.INT_ADD)for(Varnode input:op.getInputs())if(input.isAddress())indirect68020=true;
                if(indirect68020){
                    row.add("reference_pcode",ops);row.addProperty("reference_limitation","68020 full-format indexed data candidate has inconsistent address lowering between pseudo-disassembly and emulator; not supported by the 68000 adapter");
                    JsonArray stop=new JsonArray();stop.add("UNIMPLEMENTED");stop.add(JsonNull.INSTANCE);stop.add(new JsonArray());ops=new JsonArray();ops.add(stop);
                }
                row.add("ops",ops);out.println(row);instructions++;
            }
        }
        println("NATIVE_SOUND_EXPORT_COMPLETE instructions="+instructions+" invalid_candidates="+invalid+" operations="+counts);
    }
}
