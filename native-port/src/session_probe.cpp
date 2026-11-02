#include "native_session.hpp"
#include <iomanip>
#include <iostream>
#include <memory>
#include <fstream>
#include <deque>

int main(int argc,char** argv) {
    try {
        if(argc!=5&&argc!=6){std::cerr<<"Development probe: <EPROM> <raw jp1 BIOS> <mapped cartridge> <resident profile> [instruction budget]\n";return 2;}
        const auto budget=argc==6?std::stoull(argv[5]):10000000ull;if(budget==0||budget>1000000000ull)throw std::invalid_argument("Invalid development instruction budget");
        auto session=std::make_unique<tecmo98::NativeSession>(argv[1],argv[2],argv[3],argv[4]);
        std::deque<std::pair<bool,std::uint32_t>> history;
        std::ofstream soundTrace("native-sound-status.txt");unsigned oldStatus=0xffffffff,priorSoundPc=0;
        session->soundSourceTrace=[&](std::uint32_t pc){const auto& c=session->soundContext;const unsigned status=(c.abi[0x40]<<15)|(c.abi[0x41]<<13)|(c.abi[0x42]<<8);if(status!=oldStatus){soundTrace<<std::hex<<priorSoundPc<<" -> "<<pc<<" flags="<<status<<" shadow="<<tecmo98::aot::be<2>(c.abi,0x200)<<" SP="<<tecmo98::aot::be<4>(c.abi,60)<<'\n';oldStatus=status;}priorSoundPc=pc;};
        struct Jump{bool slave;std::uint32_t from,to;unsigned repeats;};std::deque<Jump> jumps;
        std::array<std::uint32_t,2> previous{};
        session->sourceTrace=[&](bool slave,std::uint32_t pc){if(history.size()==8192)history.pop_front();history.emplace_back(slave,pc);auto& old=previous[slave];if(pc!=old+2&&pc!=old+4){if(!jumps.empty()&&jumps.back().slave==slave&&jumps.back().from==old&&jumps.back().to==pc)++jumps.back().repeats;else{if(jumps.size()==4096)jumps.pop_front();jumps.push_back({slave,old,pc,1});}}old=pc;};
        std::string reason="native instruction budget exhausted";
        try {session->runCooperative(budget);}
        catch(const std::exception& error){reason=error.what();const auto& c=session->activeSlave()?session->slaveContext:session->context;std::cerr<<"PC="<<std::hex<<c.pc<<" soundPC="<<session->soundContext.pc;for(unsigned i=0;i<16;++i)std::cerr<<" R"<<std::dec<<i<<"="<<std::hex<<c.reg(i);std::cerr<<std::dec<<'\n';}
        std::ofstream trace("native-session-trace.txt");for(const auto& [slave,pc]:history)trace<<(slave?"S ":"M ")<<std::hex<<pc<<'\n';
        std::ofstream flow("native-session-flow.txt");for(const auto& jump:jumps)flow<<(jump.slave?"S ":"M ")<<std::hex<<jump.from<<" -> "<<jump.to<<" x"<<std::dec<<jump.repeats<<'\n';
        const auto view=session->videoView();
        for(const auto& [name,data]:std::array<std::pair<const char*,std::span<const std::uint8_t>>,4>{{{"native-display.bin",view.display},{"native-draw.bin",view.draw},{"native-vdp1-regs.bin",view.vdp1},{"native-vdp2-regs.bin",view.vdp2}}}){std::ofstream out(name,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());}
        for(const auto& [name,base,length]:std::array<std::tuple<const char*,std::uint32_t,unsigned>,6>{{{"native-vdp1.bin",0x05c00000,0x80000},{"native-vdp2.bin",0x05e00000,0x80000},{"native-cram.bin",0x05f00000,0x1000},{"native-workram.bin",0x06000000,0x100000},{"native-soundram.bin",0x05a00000,0x100000},{"native-lowram.bin",0x00200000,0x100000}}}){std::ofstream out(name,std::ios::binary);for(unsigned i=0;i<length;i+=4){const auto value=session->read(base+i,4);for(int shift=24;shift>=0;shift-=8)out.put(char(value>>shift));}}
        std::cout<<"{\"execution\":\"static Windows AMD64 code\",\"executed_native_source_instructions\":"<<session->context.executed
                 <<",\"slave_native_source_instructions\":"<<session->slaveContext.executed
                 <<",\"sound_native_source_instructions\":"<<session->soundContext.executed
                 <<",\"completed_control_draw_lists\":"<<session->completedDrawLists()
                 <<",\"active_context\":"<<std::quoted(session->activeSound()?"sound":session->activeSlave()?"slave":"main")
                 <<",\"stopped_at\":"<<(session->activeSound()?session->soundContext.pc:session->activeSlave()?session->slaveContext.pc:session->context.pc)<<",\"reason\":"<<std::quoted(reason)
                 <<",\"profile\":\"partial observed jp1 resident data; not cold firmware boot\",\"playable_game\":false}\n";
        return 3;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
