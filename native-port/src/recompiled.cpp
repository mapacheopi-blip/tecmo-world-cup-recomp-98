#include "recompiled.hpp"

namespace tecmo98::aot {
std::uint32_t Context::reg(unsigned index) const {
    if(index>=16)throw std::out_of_range("Invalid compatibility argument register");
    return std::uint32_t(be<4>(abi,index*4));
}
void Context::reg(unsigned index,std::uint32_t value) {
    if(index>=16)throw std::out_of_range("Invalid compatibility argument register");
    setBe<4>(abi,index*4,value);
}
std::uint32_t Context::special(unsigned offset) const {
    if(offset<0x100||offset>0x118||(offset&3)!=0)throw std::out_of_range("Invalid compatibility ABI field");
    return std::uint32_t(be<4>(abi,offset));
}
void Context::special(unsigned offset,std::uint32_t value) {
    if(offset<0x100||offset>0x118||(offset&3)!=0)throw std::out_of_range("Invalid compatibility ABI field");
    setBe<4>(abi,offset,value);
}
}
