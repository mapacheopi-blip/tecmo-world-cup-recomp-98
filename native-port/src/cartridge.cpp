#include "cartridge.hpp"

namespace tecmo98 {
CartridgeImage::CartridgeImage(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {
    if (bytes_.size()!=size) throw std::invalid_argument("Expected 48 MiB ST-V cartridge data");
}
std::span<const std::uint8_t> CartridgeImage::span(std::uint32_t cpuAddress,std::size_t length) const {
    // Cache-through aliases use the same bytes in this native data adapter.
    const auto address=cpuAddress&~0x20000000u;
    if (address<0x02000000 || length>size || std::uint64_t(address-0x02000000)+length>size)
        throw std::out_of_range("Unimplemented cartridge data address");
    return {bytes_.data()+address-0x02000000,length};
}
std::uint32_t CartridgeImage::read32(std::uint32_t address) const {
    const auto b=span(address,4);
    return (std::uint32_t(b[0])<<24)|(std::uint32_t(b[1])<<16)|(std::uint32_t(b[2])<<8)|b[3];
}
void ProtectionRegisters::write16(std::uint32_t address,std::uint16_t value) {
    if (address<0x04fffff0 || address>0x04fffffe || (address&1)!=0)
        throw std::out_of_range("Unimplemented protection register write");
    const auto index=(address-0x04fffff0)/4,shift=(address&2)==0?16u:0u;
    words_[index]=(words_[index]&~(0xffffu<<shift))|(std::uint32_t(value)<<shift);
    if (index==2) {
        if (shift==16) requestedAddressLow=value;else requestedAddressHigh=value;
    }
    if (index==3) requestedSubkey=std::uint16_t(words_[3]>>16);
}
std::uint16_t ProtectionRegisters::read16(std::uint32_t address,const CartridgeImage& cart) const {
    if (address<0x04fffff0 || address>0x04fffffe || (address&1)!=0)
        throw std::out_of_range("Unimplemented protection register read");
    const auto index=(address-0x04fffff0)/4,shift=(address&2)==0?16u:0u;
    const bool enabled=(words_[0]&0x10000)!=0;
    if (enabled && index==3) throw std::runtime_error("315-5881 encrypted stream decoder is not implemented");
    const auto word=enabled || words_[index]!=0?words_[index]:cart.read32(0x04fffff0+index*4);
    return std::uint16_t(word>>shift);
}
void sub_06009614(WorkRam& ram,const CartridgeImage& cart,ProtectionRegisters& protection) {
    const auto signature=cart.span(0x22400000,14);
    for (unsigned i=0;i<signature.size();++i)
        if (signature[i]!=ram.read8(0x06065120+i))
            throw std::runtime_error("Cartridge signature mismatch: untranslated original error-UI path at 0x06009648");
    ram.write8(0x0607931c,0);
    protection.write16(0x04fffff8,0x1234);
    if (protection.read16(0x04fffff8,cart)!=0x1234)
        throw std::runtime_error("Cartridge control register did not acknowledge initialization");
}
}
