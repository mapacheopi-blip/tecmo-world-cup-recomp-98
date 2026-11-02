#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace tecmo98::aot {
// Compatibility ABI data for statically compiled SH-2 functions. No opcode decoder,
// instruction table or p-code evaluator exists in the product.
struct Context {
    std::array<std::uint8_t,0x11c> abi{};
    std::uint32_t pc=0,stop=0;
    std::uint64_t remaining=0,executed=0;
    bool yieldRequested=false;
    std::uint32_t reg(unsigned index) const;
    void reg(unsigned index,std::uint32_t value);
    std::uint32_t special(unsigned offset) const;
    void special(unsigned offset,std::uint32_t value);
};
class DataBus {
public:
    virtual ~DataBus()=default;
    // Optional, explicitly validated game-port function replacements.
    virtual bool runNativeFunction(Context&){return false;}
    virtual std::uint64_t read(std::uint32_t address,unsigned size)=0;
    virtual void write(std::uint32_t address,unsigned size,std::uint64_t value)=0;
    virtual void verifySource(std::uint32_t address,std::uint32_t expected,unsigned bytes)=0;
};
template<unsigned Size,class Array> inline std::uint64_t be(const Array& bytes,unsigned at) {
    static_assert(Size>0&&Size<=8);std::uint64_t value=0;
    for (unsigned i=0;i<Size;++i)value=(value<<8)|bytes[at+i];return value;
}
template<unsigned Size,class Array> inline void setBe(Array& bytes,unsigned at,std::uint64_t value) {
    static_assert(Size>0&&Size<=8);for (unsigned i=0;i<Size;++i)bytes[at+i]=std::uint8_t(value>>(8*(Size-1-i)));
}
inline std::int64_t sign(std::uint64_t value,unsigned bytes) {
    const auto high=std::uint64_t(1)<<(bytes*8-1);
    if (bytes<8&&(value&high)!=0)value|=(~std::uint64_t(0))<<(bytes*8);
    return std::bit_cast<std::int64_t>(value);
}
inline std::uint64_t left(std::uint64_t value,std::uint64_t count,unsigned bytes) {return count>=bytes*8?0:value<<count;}
inline std::uint64_t right(std::uint64_t value,std::uint64_t count,unsigned bytes) {return count>=bytes*8?0:value>>count;}
inline std::uint64_t arithmetic(std::uint64_t value,std::uint64_t count,unsigned bytes) {
    const auto signedValue=sign(value,bytes);
    return count>=bytes*8?std::uint64_t(signedValue<0?~std::uint64_t(0):0):std::uint64_t(signedValue>>count);
}
// These are emitted native C++ functions. The build tool alone reads the exported IR.
void run(Context& context,DataBus& data,std::uint64_t instructionBudget);
}
