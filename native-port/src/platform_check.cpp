#include "platform.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace {
std::uint32_t u32(std::istream& input) {
    unsigned char b[4];
    if (!input.read(reinterpret_cast<char*>(b),4)) throw std::runtime_error("Truncated platform fixture");
    return b[0] | (std::uint32_t(b[1])<<8) | (std::uint32_t(b[2])<<16) | (std::uint32_t(b[3])<<24);
}
std::uint32_t be32(std::istream& input) {
    unsigned char b[4];
    if (!input.read(reinterpret_cast<char*>(b),4)) throw std::runtime_error("Truncated platform hardware state");
    return (std::uint32_t(b[0])<<24) | (std::uint32_t(b[1])<<16) | (std::uint32_t(b[2])<<8) | b[3];
}
void readRam(std::istream& input,tecmo98::WorkRam& ram) {
    if (!input.read(reinterpret_cast<char*>(ram.bytes.data()),0x2000)) throw std::runtime_error("Truncated platform RAM");
}
void checkContracts() {
    tecmo98::NativePlatform platform;int executed=0;
    if (!platform.slave.tryRun([&]{++executed;}) || executed!=1) throw std::runtime_error("Native task did not run");
    platform.sendSystemCommand(3);
    if (platform.slave.tryRun([&]{++executed;}) || executed!=1) throw std::runtime_error("Slave reset did not block native task");
    const auto completed=platform.completedSystemCommands;
    bool rejected=false;try {platform.sendSystemCommand(0x7f);} catch (const std::invalid_argument&) {rejected=true;}
    if (!rejected || platform.completedSystemCommands!=completed) throw std::runtime_error("Unsupported command accepted");
    auto ram=std::make_unique<tecmo98::WorkRam>();
    ram->bytes.fill(0x5a);const auto before=std::make_unique<tecmo98::WorkRam>(*ram);
    rejected=false;try {tecmo98::begin_reset_000034e8(*ram,platform);} catch (const std::runtime_error&) {rejected=true;}
    if (!rejected || ram->bytes!=before->bytes) throw std::runtime_error("Unknown BIOS binding changed memory");
    tecmo98::install_observed_jp1_prefix(*ram);
    const auto prepared=std::make_unique<tecmo98::WorkRam>(*ram);
    platform.systemCommandBusy=true;
    rejected=false;try {tecmo98::begin_reset_000034e8(*ram,platform);} catch (const std::runtime_error&) {rejected=true;}
    if (!rejected || ram->bytes!=prepared->bytes) throw std::runtime_error("Busy platform reset changed memory");
    rejected=false;try {tecmo98::resident_060014c0(*ram,std::span(ram->bytes).first(12));} catch (const std::invalid_argument&) {rejected=true;}
    if (!rejected || ram->bytes!=prepared->bytes) throw std::runtime_error("Invalid vector source changed memory");
}
}

int main(int argc,char** argv) {
    try {
        if (argc!=2) {std::cerr<<"Native platform development check. Usage: tecmo98-native-platform-check.exe <platform-vectors.bin>\n";return 2;}
        std::ifstream input(std::filesystem::path(argv[1]),std::ios::binary);char magic[8];
        if (!input.read(magic,8) || std::string(magic,8)!="T98PLAT1") throw std::runtime_error("Invalid platform fixture");
        const auto count=u32(input);if (count==0 || count>10000) throw std::runtime_error("Invalid platform count");
        std::array<std::uint32_t,4> kinds{};
        for (std::uint32_t i=0;i<count;i++) {
            const auto kind=u32(input),first=u32(input),second=u32(input),initialHardware=u32(input);
            auto ram=std::make_unique<tecmo98::WorkRam>(),expected=std::make_unique<tecmo98::WorkRam>();
            readRam(input,*ram);readRam(input,*expected);const auto expectedHardware=be32(input),expectedCommand=u32(input);
            tecmo98::NativePlatform platform;platform.interruptMask=initialHardware;
            if (kind==1) tecmo98::resident_06000c0a(*ram,platform,first,second);
            else if (kind==2) {
                const auto at=ram->offset(first,128);
                tecmo98::resident_060014c0(*ram,std::span<const std::uint8_t>(ram->bytes).subspan(at,128));
            } else if (kind==3) tecmo98::resident_060014a8(*ram,first,second);
            else if (kind==4) {
                const auto boundary=tecmo98::begin_reset_000034e8(*ram,platform);
                if (boundary.resumeAddress!=0x3504 || boundary.nextService!=0x40c0 || !platform.slave.resetHeld() ||
                    platform.systemCommandBusy || platform.completedSystemCommands!=1 || platform.lastSystemCommand!=expectedCommand)
                    throw std::runtime_error("Native platform prefix boundary mismatch");
            } else throw std::runtime_error("Unknown platform case");
            if (ram->bytes!=expected->bytes || platform.interruptMask!=expectedHardware) {
                const auto difference=std::mismatch(ram->bytes.begin(),ram->bytes.end(),expected->bytes.begin());
                if (difference.first!=ram->bytes.end())
                    std::cerr<<"First RAM difference: 0x"<<std::hex<<(tecmo98::WorkRam::base+std::distance(ram->bytes.begin(),difference.first))
                             <<" native="<<unsigned(*difference.first)<<" original="<<unsigned(*difference.second)<<std::dec<<'\n';
                throw std::runtime_error("Original/native platform mismatch in case "+std::to_string(i));
            }
            ++kinds[kind-1];
        }
        if (input.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("Unexpected platform trailer");
        checkContracts();
        std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"mask_cases\":"<<kinds[0]<<",\"vector_copy_cases\":"<<kinds[1]
                 <<",\"handler_cases\":"<<kinds[2]<<",\"reset_prefix_cases\":"<<kinds[3]
                 <<",\"slave_native_task_blocked\":true,\"unknown_service_rejected\":true,\"busy_reset_rejected\":true,\"unsupported_command_rejected\":true,\"invalid_vectors_rejected\":true,\"playable_game\":false}\n";
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
