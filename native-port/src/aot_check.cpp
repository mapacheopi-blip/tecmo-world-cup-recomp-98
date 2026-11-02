#include "recompiled.hpp"
#include "game_routines.hpp"
#include "image_loader.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace {
std::uint32_t u32(std::istream& stream) {
    std::uint8_t b[4];if(!stream.read(reinterpret_cast<char*>(b),4))throw std::runtime_error("Truncated AOT fixture");
    return b[0]|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
std::vector<std::uint8_t> bytes(std::istream& stream,unsigned count) {
    if(count>tecmo98::WorkRam::size)throw std::runtime_error("Invalid AOT span");
    std::vector<std::uint8_t> b(count);if(!stream.read(reinterpret_cast<char*>(b.data()),count))throw std::runtime_error("Truncated AOT span");return b;
}
class RamBus:public tecmo98::aot::DataBus {
public:
    tecmo98::WorkRam& ram;
    explicit RamBus(tecmo98::WorkRam& memory):ram(memory){}
    std::uint64_t read(std::uint32_t address,unsigned size) override {
        if(size==0||size>8)throw std::out_of_range("Invalid native data width");
        ram.offset(address,size);std::uint64_t value=0;
        for(unsigned i=0;i<size;++i)value=(value<<8)|ram.read8(address+i);return value;
    }
    void write(std::uint32_t address,unsigned size,std::uint64_t value) override {
        if(size==0||size>8)throw std::out_of_range("Invalid native data width");
        ram.offset(address,size);for(unsigned i=0;i<size;++i)ram.write8(address+i,std::uint8_t(value>>(8*(size-1-i))));
    }
    void verifySource(std::uint32_t address,std::uint32_t expected,unsigned size) override {
        if(read(address,size)!=expected)throw std::runtime_error("Native block source changed; retranslation required");
    }
};
}
int main(int argc,char** argv) {
    try {
        if(argc!=3){std::cerr<<"Usage: tecmo98-native-aot-check <original-vectors.bin> <boot-eprom.bin>\n";return 2;}
        std::ifstream input(std::filesystem::path(argv[1]),std::ios::binary);char magic[8];
        if(!input.read(magic,8)||std::string(magic,8)!="T98VEC01")throw std::runtime_error("Wrong AOT fixture");
        const auto count=u32(input);if(count==0||count>10000)throw std::runtime_error("Invalid AOT count");
        for(unsigned test=0;test<count;++test) {
            auto ram=std::make_unique<tecmo98::WorkRam>();tecmo98::load_program(*ram,std::filesystem::path(argv[2]));
            const auto kind=u32(input),gbr=u32(input),result=u32(input),spans=u32(input);
            auto expected=std::make_unique<tecmo98::WorkRam>(*ram);
            if(spans>100)throw std::runtime_error("Invalid AOT span count");
            for(unsigned span=0;span<spans;++span) {
                const auto at=u32(input),size=u32(input);const auto before=bytes(input,size),after=bytes(input,size);
                const auto offset=ram->offset(at,size);
                std::copy(before.begin(),before.end(),ram->bytes.begin()+offset);
                std::copy(after.begin(),after.end(),expected->bytes.begin()+offset);
            }
            tecmo98::aot::Context context;RamBus data(*ram);
            context.pc=kind==1?0x0605a670:kind==2?0x0605ac54:kind==3?0x06004192:0;
            if(context.pc==0)throw std::runtime_error("Unknown AOT case");
            context.stop=kind==3?0x060041aa:0x06000000;context.special(0x114,0x06000000);context.special(0x104,gbr);
            context.reg(11,0x060764d6);context.reg(12,0x060764d8);context.reg(13,0x0607651b);context.reg(14,0x06076518);
            tecmo98::aot::run(context,data,1000);
            if(context.pc!=context.stop||ram->bytes!=expected->bytes||(kind!=3&&context.reg(0)!=result))
                throw std::runtime_error("Original/AOT mismatch at case "+std::to_string(test));
        }
        if(input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected AOT trailer");
        // Guard remains active across repeated native invocations, including delay-slot bytes.
        auto ram=std::make_unique<tecmo98::WorkRam>();tecmo98::load_program(*ram,std::filesystem::path(argv[2]));RamBus bus(*ram);
        tecmo98::aot::Context c;c.pc=0x0605ac54;c.special(0x104,0x060ffc00);c.special(0x114,0x06000000);c.stop=0x06000000;
        ram->write8(c.pc,ram->read8(c.pc)^1);bool rejected=false;
        try{tecmo98::aot::run(c,bus,100);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected)throw std::runtime_error("Changed native source accepted");
        std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"source_mutation_rejected\":true,\"execution\":\"statically compiled Windows AMD64 blocks\",\"playable_game\":false}\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
