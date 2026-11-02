#include "platform.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

namespace {
std::uint32_t u32(std::istream& input) {
    unsigned char b[4];
    if (!input.read(reinterpret_cast<char*>(b),4)) throw std::runtime_error("Truncated I/O fixture");
    return b[0] | (std::uint32_t(b[1])<<8) | (std::uint32_t(b[2])<<16) | (std::uint32_t(b[3])<<24);
}
std::uint32_t be32(std::istream& input) {
    unsigned char b[4];
    if (!input.read(reinterpret_cast<char*>(b),4)) throw std::runtime_error("Truncated I/O registers");
    return (std::uint32_t(b[0])<<24) | (std::uint32_t(b[1])<<16) | (std::uint32_t(b[2])<<8) | b[3];
}
struct Hardware {
    std::uint32_t dma,port2,bus,vbr;
    std::array<std::uint32_t,48> scu;
};
Hardware hardware(std::istream& input) {
    Hardware state{u32(input),u32(input),u32(input),u32(input),{}};
    for (auto& word:state.scu) word=be32(input);
    if (state.dma>15 || state.port2>255 || state.bus>65535) throw std::runtime_error("Invalid I/O state");
    return state;
}
void ramBytes(std::istream& input,tecmo98::WorkRam& ram) {
    if (!input.read(reinterpret_cast<char*>(ram.bytes.data()),0x2000)) throw std::runtime_error("Truncated I/O RAM");
}
void contracts() {
    tecmo98::NativePlatform platform;int executed=0;
    for (unsigned byte=0;byte<256;byte++) {
        platform.writeAudioPort2(std::uint8_t(byte));
        const auto previous=executed;
        const bool ran=platform.audio.tryRun([&]{++executed;});
        const bool expected=(byte&0x10)==0;
        if (ran!=expected || executed!=previous+int(expected) || platform.audioPort2!=byte)
            throw std::runtime_error("Audio port/reset gate contract failed");
    }
    if (executed!=128) throw std::runtime_error("Audio callbacks not dispatched correctly");
    platform.cpuDma=tecmo98::CpuDmaDispatchGate(1);
    if (!platform.cpuDma.tryDispatch([&]{++executed;})) throw std::runtime_error("DMA gate did not allow enabled task");
    const auto previous=executed;tecmo98::bios_000040a8(platform);
    if (platform.cpuDma.tryDispatch([&]{++executed;}) || executed!=previous) throw std::runtime_error("Disabled DMA dispatched a native task");
    auto fresh=tecmo98::CpuDmaDispatchGate();fresh.writeOperation(6);
    if (fresh.operation()!=0) throw std::runtime_error("Software raised DMA error flags");
    auto fault=tecmo98::CpuDmaDispatchGate(7);fault.writeOperation(6);
    if (fault.operation()!=6) throw std::runtime_error("DMA reset lost preserved error flags");
    fault.writeOperation(0);if (fault.operation()!=0) throw std::runtime_error("DMA errors did not clear");
    auto ram=std::make_unique<tecmo98::WorkRam>();tecmo98::install_observed_jp1_prefix(*ram);ram->write32(0x06000340,0);
    const auto before=std::make_unique<tecmo98::WorkRam>(*ram);
    tecmo98::NativePlatform unbound;unbound.cpuDma=tecmo98::CpuDmaDispatchGate(1);
    bool refused=false;try {tecmo98::continue_reset_00003504(*ram,unbound);} catch (const std::runtime_error&) {refused=true;}
    if (!refused || ram->bytes!=before->bytes || unbound.audio.resetHeld() || unbound.cpuDma.operation()!=1 || unbound.scu.prepared)
        throw std::runtime_error("Unknown SCU service changed state");
}
}

int main(int argc,char** argv) {
    try {
        if (argc!=2) {std::cerr<<"Development test: tecmo98-native-io-check.exe <io-reset-vectors.bin>\n";return 2;}
        std::ifstream input(std::filesystem::path(argv[1]),std::ios::binary);char magic[8];
        if (!input.read(magic,8) || std::string(magic,8)!="T98IO004") throw std::runtime_error("Wrong I/O fixture");
        const auto count=u32(input);if (count==0 || count>10000) throw std::runtime_error("Invalid I/O count");
        std::array<unsigned,5> counts{};
        for (unsigned test=0;test<count;test++) {
            const auto kind=u32(input),argument=u32(input);
            auto ram=std::make_unique<tecmo98::WorkRam>(),expected=std::make_unique<tecmo98::WorkRam>();
            ramBytes(input,*ram);ramBytes(input,*expected);const auto before=hardware(input),after=hardware(input);
            tecmo98::NativePlatform platform;
            platform.cpuDma=tecmo98::CpuDmaDispatchGate(std::uint8_t(before.dma));platform.writeAudioPort2(std::uint8_t(before.port2));
            platform.scu.words=before.scu;platform.scu.regionBusControl=std::uint16_t(before.bus);platform.scu.originalVectorBase=before.vbr;
            platform.interruptMask=before.scu[0xa0/4];
            switch (kind) {
                case 1:tecmo98::bios_000040c0(*ram,platform);break;
                case 2:tecmo98::bios_000040a8(platform);break;
                case 3:tecmo98::bios_000035d8(*ram,platform);break;
                case 4:tecmo98::resident_06000c00(*ram,platform,argument);break;
                case 5:{
                    const auto first=tecmo98::begin_reset_000034e8(*ram,platform);
                    if (first.resumeAddress!=0x3504) throw std::runtime_error("Wrong first reset boundary");
                    const auto next=tecmo98::continue_reset_00003504(*ram,platform);
                    if (next.resumeAddress!=0x3510 || next.nextService!=0x40ce || !platform.slave.resetHeld() || platform.completedSystemCommands!=1)
                        throw std::runtime_error("Wrong extended reset boundary");
                    break;
                }
                default:throw std::runtime_error("Unknown I/O case");
            }
            if (ram->bytes!=expected->bytes || platform.cpuDma.operation()!=after.dma || platform.audioPort2!=after.port2 ||
                platform.scu.regionBusControl!=after.bus || platform.scu.originalVectorBase!=after.vbr || platform.scu.words!=after.scu ||
                platform.interruptMask!=after.scu[0xa0/4] || platform.audio.resetHeld()!=((after.port2&0x10)!=0))
                throw std::runtime_error("Original/native I/O mismatch at case "+std::to_string(test));
            if ((kind==3 || kind==5) && !platform.scu.prepared) throw std::runtime_error("SCU configuration not marked prepared");
            ++counts[kind-1];
        }
        if (input.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("Unexpected I/O trailer");
        contracts();
        std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"audio_reset_cases\":"<<counts[0]<<",\"cpu_dma_cases\":"<<counts[1]
                 <<",\"scu_setup_cases\":"<<counts[2]<<",\"mask_set_cases\":"<<counts[3]<<",\"extended_prefix_cases\":"<<counts[4]
                 <<",\"audio_gate_all_256_port_values\":true,\"dma_disabled_task_blocked\":true,\"unknown_scu_service_rejected\":true,\"playable_game\":false}\n";
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
