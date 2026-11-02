#include "recompiled_sound.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <vector>

namespace {
std::uint32_t u32(std::istream& in){std::uint8_t b[4];if(!in.read(reinterpret_cast<char*>(b),4))throw std::runtime_error("Truncated sound reference");return b[0]|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
class SparseBus:public tecmo98::aot::DataBus {
    const std::vector<std::uint8_t>& driver_;
public:
    std::map<std::uint32_t,std::uint8_t> written;
    explicit SparseBus(const std::vector<std::uint8_t>& driver):driver_(driver){}
    std::uint64_t read(std::uint32_t address,unsigned size)override{
        std::uint64_t value=0;
        for(unsigned i=0;i<size;++i){const auto wide=std::uint64_t(address)+i;if(wide>=0x100000000ull){value<<=8;continue;}const auto a=std::uint32_t(wide);auto found=written.find(a);value=(value<<8)|(found!=written.end()?found->second:a<driver_.size()?driver_[a]:0x5a);}
        return value;
    }
    void write(std::uint32_t address,unsigned size,std::uint64_t value)override{if(std::uint64_t(address)+size>0x100000000ull)throw std::out_of_range("Wrapping sound reference write");for(unsigned i=0;i<size;++i)written[address+i]=std::uint8_t(value>>(8*(size-1-i)));}
    void verifySource(std::uint32_t address,std::uint32_t expected,unsigned size)override{if(read(address,size)!=expected)throw std::runtime_error("Sound source mismatch");}
};
}
int main(int argc,char** argv){
    try{
        if(argc!=3){std::cerr<<"Usage: sound-instruction-vectors.bin sound-driver.bin\n";return 2;}
        std::ifstream source(argv[2],std::ios::binary);std::vector<std::uint8_t> driver(0x5d70);if(!source.read(reinterpret_cast<char*>(driver.data()),driver.size())||source.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Wrong sound source length");
        std::ifstream in(argv[1],std::ios::binary);char magic[8];if(!in.read(magic,8)||std::string(magic,8)!="T98SND7A")throw std::runtime_error("Wrong sound fixture");const auto count=u32(in);if(count==0||count>100000)throw std::runtime_error("Invalid sound fixture count");
        std::set<std::uint32_t> covered;
        for(unsigned test=0;test<count;++test){
            const auto address=u32(in),variant=u32(in),next=u32(in);(void)variant;tecmo98::aot_sound::Context c;c.pc=address;
            std::array<std::uint8_t,0x800> expected{};if(!in.read(reinterpret_cast<char*>(c.abi.data()),c.abi.size())||!in.read(reinterpret_cast<char*>(expected.data()),expected.size()))throw std::runtime_error("Truncated sound register reference");
            const auto ranges=u32(in);if(ranges>100)throw std::runtime_error("Too many sound write ranges");std::map<std::uint32_t,std::uint8_t> writes;
            for(unsigned range=0;range<ranges;++range){const auto start=u32(in),length=u32(in);if(length>128)throw std::runtime_error("Oversized sound reference write");for(unsigned byte=0;byte<length;++byte){char value;if(!in.get(value))throw std::runtime_error("Truncated sound memory reference");writes[start+byte]=std::uint8_t(value);}}
            SparseBus bus(driver);try{tecmo98::aot_sound::run(c,bus,1);}catch(const std::exception& error){throw std::runtime_error("Sound case="+std::to_string(test)+" address="+std::to_string(address)+": "+error.what());}
            for(unsigned byte=0;byte<c.abi.size();++byte){if(byte>=0x50&&byte<0x54)continue;if(c.abi[byte]!=expected[byte])throw std::runtime_error("Sound ABI mismatch case="+std::to_string(test)+" address="+std::to_string(address)+" byte="+std::to_string(byte)+" actual="+std::to_string(c.abi[byte])+" expected="+std::to_string(expected[byte]));}
            if(bus.written!=writes){std::cerr<<"actual writes:";for(const auto& [at,value]:bus.written)std::cerr<<' '<<std::hex<<at<<'='<<unsigned(value);std::cerr<<" expected:";for(const auto& [at,value]:writes)std::cerr<<' '<<std::hex<<at<<'='<<unsigned(value);std::cerr<<std::dec<<'\n';}
            if(c.pc!=next||bus.written!=writes)throw std::runtime_error("Sound control/memory mismatch case="+std::to_string(test)+" address="+std::to_string(address)+" actual_next="+std::to_string(c.pc)+" expected_next="+std::to_string(next));covered.insert(address);
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected sound reference trailer");
        // Motorola M68000PRM: MOVE SR preserves all implemented status bits;
        // ORI to CCR changes only the five condition-code flags. Ghidra's
        // one-byte packflags truncates the upper SR, so it is not our oracle.
        unsigned architectural=0;
        for(const unsigned sr:{0x2000u,0xa51bu,0x2700u,0x801fu}){
            SparseBus bus(driver);tecmo98::aot_sound::Context c;
            const auto status=[&](unsigned s){c.abi[0x40]=(s>>15)&1;c.abi[0x41]=(s>>13)&1;c.abi[0x42]=(s>>8)&7;for(unsigned i=0;i<5;++i)c.abi[0x43+i]=(s>>(4-i))&1;};
            status(sr);tecmo98::aot::setBe<4>(c.abi,60,0xa000);c.pc=0x19d8;tecmo98::aot_sound::run(c,bus,1);
            if(bus.read(0x9ffe,2)!=sr||tecmo98::aot::be<4>(c.abi,60)!=0x9ffe)throw std::runtime_error("Architectural MOVE SR stack save mismatch");
            status(0);c.pc=0x1a18;tecmo98::aot_sound::run(c,bus,1);
            if(tecmo98::aot_sound::packStatus(c)!=sr||tecmo98::aot::be<4>(c.abi,60)!=0xa000)throw std::runtime_error("Architectural MOVE stack SR restore mismatch");
            c.pc=0x15e4;tecmo98::aot_sound::run(c,bus,1);
            if(tecmo98::aot_sound::packStatus(c)!=(sr|1))throw std::runtime_error("Architectural ORI CCR changed upper SR");
            architectural+=3;
        }
        std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"architectural_status_cases\":"<<architectural<<",\"source_instructions_checked\":"<<covered.size()<<",\"abi_and_memory_checked\":true,\"playable_game\":false}\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
