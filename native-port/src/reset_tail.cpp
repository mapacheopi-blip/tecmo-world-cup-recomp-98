#include "reset_tail.hpp"

namespace tecmo98 {
FirmwareData::FirmwareData(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {
    if (bytes_.size()!=0x80000) throw std::invalid_argument("Expected 512 KiB transformed BIOS data");
}
std::span<const std::uint8_t> FirmwareData::span(std::uint32_t address, std::size_t size) const {
    if (address>bytes_.size() || size>bytes_.size()-address) throw std::out_of_range("Unimplemented BIOS data address");
    return {bytes_.data()+address,size};
}
std::uint32_t FirmwareData::read32(std::uint32_t address) const {
    const auto b=span(address,4);
    return (std::uint32_t(b[0])<<24)|(std::uint32_t(b[1])<<16)|(std::uint32_t(b[2])<<8)|b[3];
}

void bios_000040ce(NativeCodePolicy& code) {
    code.resetRequests.clear();
    const auto control=[&](std::uint8_t value) {
        code.originalControl=value;
        code.resetRequests.push_back({0xfffffe92,value,1});
    };
    for (unsigned i=0;i<4;++i) control(0);
    for (unsigned way=0;way<4;++way) {
        for (unsigned i=0;i<4;++i) control(std::uint8_t(way<<6));
        for (unsigned line=0;line<64;++line) code.resetRequests.push_back({0x60000000+line*16,0,4});
    }
    // There are no guest instruction caches to flush. All native bindings validate current
    // source data before invocation (CodeRegistry); invalidate the audit generation as well.
    ++code.invalidationGeneration;
    for (unsigned i=0;i<4;++i) control(1);
}

std::uint32_t bios_00003020(OnChipResetConfiguration& config, std::uint32_t argument) {
    const auto word=[&](unsigned at,std::uint16_t value) {
        config.requested[at]=std::uint8_t(value>>8); config.requested[at+1]=std::uint8_t(value);
    };
    const auto number=[&](unsigned at,std::uint32_t value) {
        word(at,std::uint16_t(value>>16));word(at+2,std::uint16_t(value));
    };
    config.originalGbr=0xfffffe00;
    word(0xe0,std::uint16_t(argument));word(0xe2,0);
    word(0x60,0x0100);word(0x62,0x6061);word(0x64,0x6263);
    word(0x66,0x6465);word(0x68,0x6600);word(0xe4,0x6869);
    number(0x1a8,0x6c);number(0x1a0,0x6d);number(0x10c,0x6e);
    config.requested[0x10]=0x81;config.prepared=true;
    return 0x81;
}

std::uint32_t bios_00000d10(WorkRam& ram,const FirmwareData& bios,std::uint32_t descriptor) {
    const auto read=[&](std::uint32_t address) {
        return address<0x80000?bios.read32(address):ram.read32(address);
    };
    if ((descriptor&3)!=0) throw std::invalid_argument("Unaligned BIOS copy descriptor");
    if (descriptor<0x80000) bios.span(descriptor,12); else ram.offset(descriptor,12);
    const auto count=read(descriptor),destination=read(descriptor+4),source=read(descriptor+8);
    if (count==0 || count>WorkRam::size/4) throw std::out_of_range("Unsupported BIOS copy count");
    if ((source&3)!=0 || (destination&3)!=0) throw std::invalid_argument("Unaligned BIOS word copy");
    const auto length=std::size_t(count)*4;
    ram.offset(destination,length);
    if (source<0x80000) bios.span(source,length); else ram.offset(source,length);
    for (std::uint32_t i=0;i<count;++i) ram.write32(destination+i*4,read(source+i*4));
    return descriptor+12;
}

void continue_reset_00003510(WorkRam& ram,const FirmwareData& bios,
                            NativeCodePolicy& code,OnChipResetConfiguration& onChip) {
    if (ram.read32(0x06000280)!=0x060014c0 || ram.read32(0x06000300)!=0x060014a8)
        throw std::runtime_error("Unresolved BIOS vector-service binding in reset tail");
    bios_000040ce(code);
    bios_00003020(onChip,1);
    resident_060014c0(ram,bios.span(0x370,128));
    resident_060014a8(ram,0x40,ram.read32(0x06000618));
    ram.write32(0x06000100,bios.read32(0x700));
    auto next=bios_00000d10(ram,bios,0x354c);
    bios_00000d10(ram,bios,next);
}

DisplayResetResult finish_display_000034ca(DisplayResetState& display) {
    const auto extend=[](std::uint16_t n) {return (n&0x8000)!=0?0xffff0000u|n:std::uint32_t(n);};
    if ((display.control&0x8000)==0) return {true,extend(display.control)};
    if ((display.status&8)==0) return {false,extend(display.status)};
    const auto result=extend(display.status);
    display.control&=1;
    return {true,result};
}
}
