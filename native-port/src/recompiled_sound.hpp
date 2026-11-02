#pragma once
#include "recompiled.hpp"
#include <limits>
namespace tecmo98::aot_sound {
using aot::DataBus;using aot::be;using aot::setBe;using aot::sign;using aot::left;using aot::right;using aot::arithmetic;
// Separate ABI for the statically compiled 68000 sound driver, including Ghidra
// flag/temporary registers. This contains values only, no instruction decoder.
struct Context {
    std::array<std::uint8_t,0x800> abi{};
    std::uint32_t pc=0,stop=0xffffffff;
    std::uint64_t remaining=0,executed=0;
    bool yieldRequested=false;
};
inline std::uint64_t mask(unsigned bytes){return bytes==8?~0ull:(1ull<<(bytes*8))-1;}
inline bool carry(std::uint64_t a,std::uint64_t b,unsigned bytes){return a>mask(bytes)-b;}
inline bool signedCarry(std::uint64_t a,std::uint64_t b,unsigned bytes){return ((~(a^b)&(a^(a+b)))&(1ull<<(bytes*8-1)))!=0;}
inline bool signedBorrow(std::uint64_t a,std::uint64_t b,unsigned bytes){return (((a^b)&(a^(a-b)))&(1ull<<(bytes*8-1)))!=0;}
inline std::uint64_t divide(std::uint64_t a,std::uint64_t b,unsigned bytes,bool isSigned,bool remainder){
    if(b==0)throw std::runtime_error("Native sound arithmetic division by zero");
    if(!isSigned)return remainder?a%b:a/b;
    const auto x=sign(a,bytes),y=sign(b,bytes);
    if(x==std::numeric_limits<std::int64_t>::min()&&y==-1)throw std::runtime_error("Native sound arithmetic division overflow");
    return std::uint64_t(remainder?x%y:x/y);
}
void run(Context&,DataBus&,std::uint64_t budget);
inline std::uint16_t packStatus(const Context& c){unsigned sr=(c.abi[0x40]<<15)|(c.abi[0x41]<<13)|(c.abi[0x42]<<8);for(unsigned i=0;i<5;++i)sr|=c.abi[0x43+i]<<(4-i);return std::uint16_t(sr);}
inline void orCcr(Context& c,std::uint8_t immediate){
    for(unsigned i=0;i<5;++i)c.abi[0x43+i]|=(immediate>>(4-i))&1;
    setBe<2>(c.abi,0x200,packStatus(c));
}
}
