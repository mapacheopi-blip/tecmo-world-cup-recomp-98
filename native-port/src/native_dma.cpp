#include "native_dma.hpp"

namespace tecmo98 {
bool transfer_cpu_dma(NativeDmaMemory& memory,std::array<std::uint32_t,4>& channel,std::uint32_t operation) {
    const auto control=channel[3];
    if((control&operation&1)==0||(control&2)!=0)return false;
    if((operation&6)!=0)throw std::runtime_error("CPU DMA error-state handling pending");
    if((control&4)!=0)throw std::runtime_error("CPU DMA completion interrupt delivery pending");
    if((control&0x300)!=0x200)throw std::runtime_error("External CPU DMA request service pending");
    const auto sizeCode=(control>>10)&3,sourceMode=(control>>12)&3,destinationMode=(control>>14)&3;
    if(sourceMode==3||destinationMode==3)throw std::runtime_error("CPU DMA reserved addressing mode pending");
    if(sizeCode==3){
        const auto count=channel[2]&0xffffff;
        if(sourceMode!=1||count==0||(count&3)!=0||count>WorkRam::size/4)throw std::runtime_error("CPU DMA extended/unaligned burst count or source mode pending");
        auto source=(channel[0]&~0x20000000u)&~3u,destination=(channel[1]&~0x20000000u)&~3u;
        const auto length=count*4;
        memory.validateRange(source,length,false);
        if(destinationMode==2&&destination<length)throw std::out_of_range("CPU burst DMA destination underflow");
        memory.validateRange(destinationMode==2?destination-length:destination,destinationMode==0?16:length,true);
        for(unsigned n=0;n<count;n+=4){
            if(destinationMode==2)destination-=16;
            for(unsigned i=0;i<4;++i)memory.write(destination+i*4,4,memory.read(source+i*4,4));
            source+=16;if(destinationMode==1)destination+=16;
        }
        channel[2]=0;channel[3]|=2;return true;
    }
    const auto width=1u<<sizeCode,count=channel[2]&0xffffff;
    if(count==0||count>WorkRam::size/width)throw std::runtime_error("Extended CPU DMA transfer count pending");
    auto source=(channel[0]&~0x20000000u)&~(width-1),destination=(channel[1]&~0x20000000u)&~(width-1);
    const auto validate=[&](std::uint32_t address,unsigned mode,bool writable) {
        const auto length=mode==0?width:std::size_t(count)*width;
        if(mode==2){if(address<length)throw std::out_of_range("CPU DMA address underflow");address-=std::uint32_t(length);}
        memory.validateRange(address,length,writable);
    };
    validate(source,sourceMode,false);validate(destination,destinationMode,true);
    for(unsigned i=0;i<count;++i) {
        if(sourceMode==2)source-=width;if(destinationMode==2)destination-=width;
        const auto value=memory.read(source,width);memory.write(destination,width,value);
        if(sourceMode==1)source+=width;if(destinationMode==1)destination+=width;
    }
    // The reference keeps SAR/DAR readable values, clears TCR and sets TE at completion.
    channel[2]=0;channel[3]|=2;return true;
}
bool transfer_cpu_dma_high_ram(WorkRam& ram,std::array<std::uint32_t,4>& channel,std::uint32_t operation) {
    class RamMemory final:public NativeDmaMemory {
        WorkRam& ram_;
    public:
        explicit RamMemory(WorkRam& memory):ram_(memory){}
        std::uint64_t read(std::uint32_t a,unsigned size)override{return size==1?ram_.read8(a):size==2?ram_.read16(a):ram_.read32(a);}
        void write(std::uint32_t a,unsigned size,std::uint64_t value)override{
            if(size==1)ram_.write8(a,std::uint8_t(value));else if(size==2)ram_.write16(a,std::uint16_t(value));else ram_.write32(a,std::uint32_t(value));
        }
        void validateRange(std::uint32_t address,std::size_t size,bool)override{ram_.offset(address,size);}
    } memory(ram);
    return transfer_cpu_dma(memory,channel,operation);
}
}
