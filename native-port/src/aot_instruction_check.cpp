#include "recompiled.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <vector>

namespace {
std::uint32_t u32(std::istream& in){std::uint8_t b[4];if(!in.read(reinterpret_cast<char*>(b),4))throw std::runtime_error("Truncated instruction fixture");return b[0]|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
std::vector<std::uint8_t> file(const std::filesystem::path& path,std::size_t expected){std::ifstream in(path,std::ios::binary);std::vector<std::uint8_t>b(expected);if(!in.read(reinterpret_cast<char*>(b.data()),b.size())||in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Invalid instruction reference source file");return b;}
class SparseBus:public tecmo98::aot::DataBus {
    const std::vector<std::uint8_t>& eprom_;const std::vector<std::uint8_t>& bios_;const std::vector<std::uint8_t>& profile_;
public:
    std::map<std::uint32_t,std::uint8_t> written;
    SparseBus(const auto& eprom,const auto& bios,const auto& profile):eprom_(eprom),bios_(bios),profile_(profile){}
    std::uint64_t read(std::uint32_t address,unsigned size)override {
        // The reference's unspecified cross-boundary reads are filled entirely with zero.
        // Production rejects wrapping transfers; this is not a physical-device model.
        if(size==0||size>8)throw std::out_of_range("Invalid sparse reference read");
        if(std::uint64_t(address)+size>0x100000000ull)return 0;
        std::uint64_t value=0;for(unsigned i=0;i<size;++i){const auto a=address+i;const auto mapped=a<0x40000000?a&~0x20000000u:a;std::uint8_t b=0;const auto found=written.find(a);
            if(found!=written.end())b=found->second;
            else if(mapped<bios_.size())b=bios_[mapped];
            else if(mapped>=0x06000000&&mapped<0x06004000)b=profile_[mapped-0x06000000];
            else if(mapped>=0x06004000&&mapped<0x060fc000)b=eprom_[mapped-0x06000000];
            value=(value<<8)|b;}return value;
    }
    void write(std::uint32_t address,unsigned size,std::uint64_t value)override {
        if(size==0||size>8||std::uint64_t(address)+size>0x100000000ull)throw std::out_of_range("Unimplemented wrapping data transfer");
        for(unsigned i=0;i<size;++i)written[address+i]=std::uint8_t(value>>(8*(size-1-i)));
    }
    void verifySource(std::uint32_t address,std::uint32_t expected,unsigned size)override{if(read(address,size)!=expected)throw std::runtime_error("Instruction source mismatch");}
};
}
int main(int argc,char** argv){
    try{
        if(argc!=5){std::cerr<<"Usage: instruction-vectors.bin boot-eprom.bin bios-be.bin profile.bin\n";return 2;}
        const auto eprom=file(argv[2],0x100000),bios=file(argv[3],0x80000),profile=file(argv[4],0x100000);
        std::ifstream in(std::filesystem::path(argv[1]),std::ios::binary);char magic[8];
        if(!in.read(magic,8)||std::string(magic,8)!="T98AOT6A")throw std::runtime_error("Wrong instruction fixture");
        const auto count=u32(in);if(count==0||count>1000000)throw std::runtime_error("Invalid instruction count");
        std::set<std::uint32_t> covered;unsigned architecturalMacCases=0,architecturalSelfPredecrementCases=0;
        for(unsigned test=0;test<count;++test){
            const auto address=u32(in),variant=u32(in),next=u32(in);(void)variant;tecmo98::aot::Context c;c.pc=address;c.stop=0xffffffff;
            for(unsigned index=0;index<22;++index){const auto value=u32(in);if(index<16)c.reg(index,value);else c.special(0x100+(index-16)*4,value);}
            std::array<std::uint32_t,22> expected{};for(auto& value:expected)value=u32(in);
            const auto ranges=u32(in);if(ranges>100)throw std::runtime_error("Invalid instruction write count");
            std::map<std::uint32_t,std::uint8_t> expectedWrites;
            for(unsigned range=0;range<ranges;++range){const auto start=u32(in),size=u32(in);if(size>64)throw std::runtime_error("Invalid instruction write range");for(unsigned i=0;i<size;++i){char value;if(!in.get(value))throw std::runtime_error("Truncated instruction writes");expectedWrites[start+i]=std::uint8_t(value);}}
            SparseBus bus(eprom,bios,profile);const auto oldHigh=c.special(0x10c),oldLow=c.special(0x110);
            const auto before=c;const auto first=unsigned(bus.read(address,2)),second=unsigned(bus.read(address+2,2));
            const bool delay=(first&0xf000)==0xa000||(first&0xf000)==0xb000||(first&0xf0ff)==0x400b||(first&0xf0ff)==0x402b||first==0xb||first==0x2b||(first&0xf0ff)==0x0003||(first&0xf0ff)==0x0023||(first&0xff00)==0x8d00||(first&0xff00)==0x8f00;
            try{tecmo98::aot::run(c,bus,1);}catch(const std::exception& error){throw std::runtime_error("Instruction case="+std::to_string(test)+" address="+std::to_string(address)+": "+error.what());}
            // Independently check MAC.L with signed host arithmetic instead of
            // accepting the known erroneous Ghidra carry reference. Source
            // words are inspected by this development checker only.
            auto opcode=unsigned(bus.read(address,2));if((opcode&0xf00f)!=0x000f)opcode=unsigned(bus.read(address+2,2));
            if((opcode&0xf00f)==0x000f&&((first&0xf00f)==0x000f||delay)){
                const unsigned n=(opcode>>8)&15,m=(opcode>>4)&15;
                const auto a=std::int64_t(std::int32_t(bus.read(c.reg(n)-(n==m?8:4),4))),b=std::int64_t(std::int32_t(bus.read(c.reg(m)-4,4)));
                if((c.special(0x100)&2)!=0){const __int128 accumulator=__int128(std::int16_t(oldHigh))*0x100000000ll+oldLow;
                    const auto sum=std::min<__int128>((__int128(1)<<47)-1,std::max<__int128>(-(__int128(1)<<47),accumulator+__int128(a)*b));
                    expected[19]=(oldHigh&0xffff0000)|(std::uint32_t(std::uint64_t(sum)>>32)&0xffff);expected[20]=std::uint32_t(sum);
                }else {const auto sum=((std::uint64_t(oldHigh)<<32)|oldLow)+std::uint64_t(a*b);expected[19]=std::uint32_t(sum>>32);expected[20]=std::uint32_t(sum);}
                ++architecturalMacCases;
            }
            // Renesas REJ09B0171-0500O p198: MOV.B/W/L Rm,@-Rn
            // writes the ORIGINAL Rm, then decrements Rn. Ghidra gets m==n wrong.
            const auto self=[](unsigned op){const auto form=op&0xf00f;return (form==0x2004||form==0x2005||form==0x2006)&&((op>>4)&15)==((op>>8)&15);};
            const auto selfOpcode=self(first)?first:delay&&self(second)?second:0;
            if(selfOpcode){const unsigned n=(selfOpcode>>8)&15,size=1u<<((selfOpcode&15)-4);
                const auto original=before.reg(n)+(first==0x2b&&n==15?8u:0u),destination=original-size;
                if(expected[n]!=destination||expectedWrites.size()!=size)throw std::runtime_error("Unexpected raw predecrement reference");
                for(unsigned b=0;b<size;++b){const auto key=destination+b;const auto found=expectedWrites.find(key);
                    if(found==expectedWrites.end()||found->second!=std::uint8_t(destination>>(8*(size-b-1))))throw std::runtime_error("Raw predecrement reference mismatch");
                    found->second=std::uint8_t(original>>(8*(size-b-1)));
                }
                ++architecturalSelfPredecrementCases;
            }
            for(unsigned index=0;index<22;++index){const auto actual=index<16?c.reg(index):c.special(0x100+(index-16)*4);
                if(actual!=expected[index])throw std::runtime_error("Instruction register mismatch case="+std::to_string(test)+" address="+std::to_string(address)+" field="+std::to_string(index)+" actual="+std::to_string(actual)+" expected="+std::to_string(expected[index]));}
            if(c.pc!=next||bus.written!=expectedWrites)throw std::runtime_error("Instruction control/memory mismatch case="+std::to_string(test)+" address="+std::to_string(address)+" actual_next="+std::to_string(c.pc)+" expected_next="+std::to_string(next));
            covered.insert(address);
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected instruction trailer");
        std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"architectural_mac_cases\":"<<architecturalMacCases<<",\"architectural_self_predecrement_cases\":"<<architecturalSelfPredecrementCases<<",\"source_instructions_checked\":"<<covered.size()<<",\"all_22_abi_fields_checked\":true,\"memory_write_sets_checked\":true,\"playable_game\":false}\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
