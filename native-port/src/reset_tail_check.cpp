#include "reset_tail.hpp"
#include "code_registry.hpp"
#include "image_loader.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

namespace {
std::uint32_t u32(std::istream& input) {
    unsigned char b[4];if (!input.read(reinterpret_cast<char*>(b),4)) throw std::runtime_error("Truncated tail fixture");
    return b[0]|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
struct Hardware {
    std::array<std::uint8_t,0x200> chip;
    std::uint32_t control,status,gbr;
    std::array<std::uint8_t,0xc0> scu;
    std::uint32_t bus,port;
};
Hardware hardware(std::istream& input) {
    Hardware hw{};if (!input.read(reinterpret_cast<char*>(hw.chip.data()),hw.chip.size())) throw std::runtime_error("Truncated tail hardware");
    hw.control=u32(input);hw.status=u32(input);hw.gbr=u32(input);
    if (!input.read(reinterpret_cast<char*>(hw.scu.data()),hw.scu.size())) throw std::runtime_error("Truncated tail SCU");
    hw.bus=u32(input);hw.port=u32(input);return hw;
}
void contracts(const tecmo98::FirmwareData& bios) {
    auto ram=std::make_unique<tecmo98::WorkRam>();
    tecmo98::CodeRegistry registry;unsigned called=0;
    const std::array<std::uint8_t,4> bytes={1,2,3,4};
    std::copy(bytes.begin(),bytes.end(),ram->bytes.begin()+0x4000);
    registry.bind(0x06004000,0x06004000,bytes,[&]{++called;});registry.invoke(0x06004000,*ram);
    ram->write8(0x06004003,5);bool rejected=false;
    try {registry.invoke(0x06004000,*ram);} catch (const std::runtime_error&) {rejected=true;}
    if (!rejected || called!=1) throw std::runtime_error("Changed source executed native binding");
    ram->write8(0x06004003,4);registry.invoke(0x06004000,*ram);
    rejected=false;try {registry.invoke(0x06009614,*ram);} catch (const std::runtime_error&) {rejected=true;}
    if (!rejected || called!=2) throw std::runtime_error("Unknown native entry was executed");
    tecmo98::DisplayResetState display{0x8001,0};
    if (tecmo98::finish_display_000034ca(display).completed || display.control!=0x8001)
        throw std::runtime_error("Display wait accepted without blank event");
    display.status=8;
    if (!tecmo98::finish_display_000034ca(display).completed || display.control!=1)
        throw std::runtime_error("Display reset did not complete on blank event");
    tecmo98::install_observed_jp1_prefix(*ram);ram->write32(0x06000280,0);
    const auto before=std::make_unique<tecmo98::WorkRam>(*ram);tecmo98::NativeCodePolicy code;tecmo98::OnChipResetConfiguration chip;
    rejected=false;try {tecmo98::continue_reset_00003510(*ram,bios,code,chip);} catch (const std::runtime_error&) {rejected=true;}
    if (!rejected || ram->bytes!=before->bytes || code.invalidationGeneration!=0 || chip.prepared)
        throw std::runtime_error("Unknown vector binding changed reset state");
    for (auto count: {0u,0x40001u,0xffffffffu}) {
        ram->write32(0x06001000,count);ram->write32(0x06001004,0x06001100);ram->write32(0x06001008,0x600);
        const auto snapshot=std::make_unique<tecmo98::WorkRam>(*ram);rejected=false;
        try {tecmo98::bios_00000d10(*ram,bios,0x06001000);} catch (const std::out_of_range&) {rejected=true;}
        if (!rejected || snapshot->bytes!=ram->bytes) throw std::runtime_error("Unsafe copy count modified memory");
    }
    ram->write32(0x06001000,2);ram->write32(0x06001004,0x060ffffc);
    const auto snapshot=std::make_unique<tecmo98::WorkRam>(*ram);rejected=false;
    try {tecmo98::bios_00000d10(*ram,bios,0x06001000);} catch (const std::out_of_range&) {rejected=true;}
    if (!rejected || snapshot->bytes!=ram->bytes) throw std::runtime_error("Copy bounds check came after writes");
}
}
int main(int argc,char** argv) {
    try {
        if (argc!=3) {std::cerr<<"Usage: tecmo98-native-reset-check <tail-vectors.bin> <raw-jp1-bios.bin>\n";return 2;}
        const auto bios=tecmo98::load_jp1_bios(std::filesystem::path(argv[2]));
        std::ifstream input(std::filesystem::path(argv[1]),std::ios::binary);char magic[8];
        if (!input.read(magic,8) || std::string(magic,8)!="T98TAIL5") throw std::runtime_error("Wrong tail fixture");
        const auto count=u32(input);if (count==0 || count>10000) throw std::runtime_error("Invalid tail count");
        for (unsigned test=0;test<count;++test) {
            const auto kind=u32(input),argument=u32(input),descriptor=u32(input),result=u32(input);
            auto ram=std::make_unique<tecmo98::WorkRam>(),expected=std::make_unique<tecmo98::WorkRam>();
            if (!input.read(reinterpret_cast<char*>(ram->bytes.data()),0x1800) || !input.read(reinterpret_cast<char*>(expected->bytes.data()),0x1800)) throw std::runtime_error("Truncated tail RAM");
            const auto before=hardware(input),after=hardware(input);
            tecmo98::NativeCodePolicy code;code.originalControl=before.chip[0x92];
            tecmo98::OnChipResetConfiguration chip;chip.requested=before.chip;chip.originalGbr=before.gbr;
            tecmo98::DisplayResetState display{std::uint16_t(before.control),std::uint16_t(before.status)};
            tecmo98::NativePlatform platform;
            for (unsigned i=0;i<platform.scu.words.size();++i) {
                const auto at=i*4;platform.scu.words[i]=(std::uint32_t(before.scu[at])<<24)|(std::uint32_t(before.scu[at+1])<<16)|(std::uint32_t(before.scu[at+2])<<8)|before.scu[at+3];
            }
            platform.scu.regionBusControl=std::uint16_t(before.bus);platform.writeAudioPort2(std::uint8_t(before.port));
            switch (kind) {
                case 1:tecmo98::bios_000040ce(code);break;
                case 2:if (tecmo98::bios_00003020(chip,argument)!=result) throw std::runtime_error("Interrupt setup result mismatch");break;
                case 3:if (tecmo98::bios_00000d10(*ram,bios,descriptor)!=result) throw std::runtime_error("Copy result mismatch");break;
                case 4:tecmo98::continue_reset_00003510(*ram,bios,code,chip);if (result!=0x3564) throw std::runtime_error("Reset descriptor mismatch");break;
                case 5:{const auto native=tecmo98::finish_display_000034ca(display);if (!native.completed || native.originalResult!=result) throw std::runtime_error("Display reset result mismatch");break;}
                case 6:{
                    const auto dma=(std::uint32_t(before.chip[0x1b2])<<8)|before.chip[0x1b3];
                    platform.cpuDma=tecmo98::CpuDmaDispatchGate(std::uint8_t(dma));
                    tecmo98::begin_reset_000034e8(*ram,platform);tecmo98::continue_reset_00003504(*ram,platform);
                    tecmo98::continue_reset_00003510(*ram,bios,code,chip);
                    const auto native=tecmo98::finish_display_000034ca(display);
                    if (!native.completed || native.originalResult!=result || !platform.slave.resetHeld() || platform.completedSystemCommands!=1) throw std::runtime_error("Full reset result mismatch");
                    const auto operation=platform.cpuDma.operation();
                    for (unsigned i=0;i<4;++i) chip.requested[0x1b0+i]=std::uint8_t(operation>>(24-8*i));
                    break;
                }
                default:throw std::runtime_error("Unknown tail case");
            }
            const auto requests=u32(input);if (requests>1000) throw std::runtime_error("Invalid cache request count");
            std::vector<tecmo98::CacheWrite> original;
            for (unsigned i=0;i<requests;++i) original.push_back({u32(input),u32(input),u32(input)});
            if (code.resetRequests!=original) throw std::runtime_error("Cache request order mismatch at case "+std::to_string(test));
            if (kind==1 || kind==4 || kind==6) chip.requested[0x92]=code.originalControl;
            if (ram->bytes!=expected->bytes || chip.requested!=after.chip || chip.originalGbr!=after.gbr || display.control!=after.control || display.status!=after.status)
                throw std::runtime_error("Original/native tail mismatch at case "+std::to_string(test));
            for (unsigned i=0;i<platform.scu.words.size();++i) {
                const auto word=platform.scu.words[i];for (unsigned byte=0;byte<4;++byte)
                    if (std::uint8_t(word>>(24-8*byte))!=after.scu[i*4+byte]) throw std::runtime_error("Full reset SCU mismatch");
            }
            if (platform.scu.regionBusControl!=after.bus || platform.audioPort2!=after.port) throw std::runtime_error("Full reset I/O mismatch");
        }
        if (input.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("Unexpected tail trailer");
        contracts(bios);
        std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"source_mutation_rejected\":true,\"unknown_entry_rejected\":true,\"display_wait_yields\":true,\"copy_bounds_checked_before_writes\":true,\"playable_game\":false}\n";
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
