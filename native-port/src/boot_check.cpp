#include "boot.hpp"
#include "image_loader.hpp"
#include "platform.hpp"
#include "reset_tail.hpp"
#include "code_registry.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace {
std::uint32_t number(std::istream& stream) {
    std::array<unsigned char,4> b{};
    if (!stream.read(reinterpret_cast<char*>(b.data()),4)) throw std::runtime_error("Truncated boot fixture");
    return b[0] | (std::uint32_t(b[1])<<8) | (std::uint32_t(b[2])<<16) | (std::uint32_t(b[3])<<24);
}
std::vector<std::uint8_t> readBytes(std::istream& stream,std::uint32_t size) {
    if (size > tecmo98::WorkRam::size) throw std::runtime_error("Invalid boot span size");
    std::vector<std::uint8_t> bytes(size);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()),size)) throw std::runtime_error("Truncated boot span");
    return bytes;
}
int probe(const std::filesystem::path& eprom,const std::filesystem::path& rawBios,const std::filesystem::path& mappedCart) {
    auto ram=std::make_unique<tecmo98::WorkRam>();
    // Non-zero fill demonstrates the translated clear; low RAM is not a BIOS snapshot.
    ram->bytes.fill(0xa5);
    const auto loaded=tecmo98::load_program(*ram,eprom);
    const auto bios=tecmo98::load_jp1_bios(rawBios);
    const auto cart=tecmo98::load_cartridge(mappedCart);
    tecmo98::ProtectionRegisters protection;
    tecmo98::CodeRegistry registry;
    tecmo98::BootHandoff handoff{};
    std::uint32_t firstGameService=0;
    // These bytes come from the already hash-verified EPROM, before any initialization writes.
    registry.bind(0x060540a4,0x060540a4,std::span(ram->bytes).subspan(0x540a4,0x20),[&] {
        handoff=tecmo98::handoff_060540a4(*ram,0x06004108);
    });
    registry.bind(0x06004108,0x06004108,std::span(ram->bytes).subspan(0x4108,0xf0),[&] {
        // [0x06004108,0x0600410a): load the first service ID, stop before calling it.
        firstGameService=ram->read32(0x060041f4);
    });
    registry.bind(0x06009614,0x06009614,std::span(ram->bytes).subspan(0x9614,0x140),[&] {
        tecmo98::sub_06009614(*ram,cart,protection);
    });
    tecmo98::install_observed_jp1_prefix(*ram);
    std::vector<std::uint8_t> video(tecmo98::video_storage_size,0xa5);
    tecmo98::clear_06004002(*ram,video);
    tecmo98::NativePlatform platform;
    const auto first=tecmo98::begin_reset_000034e8(*ram,platform);
    if (first.nextService!=0x40c0) throw std::runtime_error("Unknown reset boundary");
    const auto next=tecmo98::continue_reset_00003504(*ram,platform);
    if (next.nextService!=0x40ce) throw std::runtime_error("Unknown I/O reset boundary");
    tecmo98::NativeCodePolicy code;
    tecmo98::OnChipResetConfiguration onChip;
    tecmo98::continue_reset_00003510(*ram,bios,code,onChip);
    // Native cold surface begins with display disabled; no fabricated vblank event is supplied.
    tecmo98::DisplayResetState display;
    const auto displayReset=tecmo98::finish_display_000034ca(display);
    if (!displayReset.completed) throw std::runtime_error("Display reset still waiting for a blank event");
    registry.invoke(0x060540a4,*ram);
    code.originalControl=std::uint8_t(handoff.controlValue);
    ++code.invalidationGeneration; // Trampoline requests purge/enable; no host instruction bytes are generated.
    registry.invoke(handoff.entry,*ram);
    registry.invoke(firstGameService,*ram);
    // [0x0600410e,0x06004110): resolve the next original indirect call; never skip it.
    const auto nextGameService=ram->read32(0x060041f8);
    std::cout << "{\"execution\":\"native Windows x64\",\"eprom_sha1\":\"" << loaded.epromSha1
              << "\",\"program_bytes_loaded\":" << loaded.bytes
              << ",\"work_bytes_cleared\":" << tecmo98::WorkRam::base+tecmo98::WorkRam::size-tecmo98::startup_clear_begin
              << ",\"video_data_bytes_cleared\":" << video.size()
              << ",\"native_interrupt_mask\":" << platform.interruptMask
              << ",\"boot_control_flags\":" << ram->read32(0x06000824)
              << ",\"completed_system_commands\":" << platform.completedSystemCommands
              << ",\"slave_task_dispatch_enabled\":" << (platform.slave.resetHeld()?"false":"true")
              << ",\"audio_task_dispatch_enabled\":" << (platform.audio.resetHeld()?"false":"true")
              << ",\"audio_port2\":" << unsigned(platform.audioPort2)
              << ",\"cpu_dma_operation\":" << platform.cpuDma.operation()
              << ",\"scu_configuration_prepared\":" << (platform.scu.prepared?"true":"false")
              << ",\"scu_timer0_compare\":" << platform.scu.words[0x90/4]
              << ",\"scu_timer1_reload\":" << platform.scu.words[0x94/4]
              << ",\"scu_timer_mode\":" << platform.scu.words[0x98/4]
              << ",\"original_vector_base\":" << platform.scu.originalVectorBase
              << ",\"firmware_reset_completed\":true,\"cache_reset_request_count\":" << code.resetRequests.size()
              << ",\"native_code_invalidation_generation\":" << code.invalidationGeneration
              << ",\"original_cache_control\":" << unsigned(code.originalControl)
              << ",\"onchip_interrupt_configuration_prepared\":" << (onChip.prepared?"true":"false")
              << ",\"handoff_entry\":" << handoff.entry << ",\"handoff_original_stack\":" << handoff.originalStack
              << ",\"handoff_gbr\":" << handoff.gbr << ",\"first_game_service\":" << firstGameService
              << ",\"cartridge_bytes_loaded\":" << tecmo98::CartridgeImage::size
              << ",\"cartridge_initial_check_completed\":true,\"protection_address_low_request\":" << protection.requestedAddressLow
              << ",\"next_game_service\":" << nextGameService
              << ",\"profile\":\"observed jp1 prefix data only; not full firmware state\""
                 ",\"stopped_before\":\"game initialization service 0x060056f0 at call 0x06004110\""
                 ",\"platform_prefix_implemented\":true,\"io_reset_implemented\":true,\"platform_service_implemented\":false,\"playable_game\":false}\n";
    return 3; // Expected incomplete boot boundary; not a successful game launch.
}
}

int main(int argc,char** argv) {
    try {
        if (argc==5 && std::string(argv[1])=="--probe") return probe(std::filesystem::path(argv[2]),std::filesystem::path(argv[3]),std::filesystem::path(argv[4]));
        if (argc!=2) {std::cerr << "Development check, not a playable port.\nUsage: tecmo98-native-boot-check.exe <boot-vectors.bin> | --probe <game-eprom.bin> <raw-jp1-bios.bin> <mapped-cart.bin>\n";return 2;}
        std::ifstream input(std::filesystem::path(argv[1]),std::ios::binary);
        char magic[8];
        if (!input.read(magic,8) || std::string(magic,8)!="T98BOOT1") throw std::runtime_error("Wrong boot fixture");
        const auto cases=number(input);
        if (cases==0 || cases>1000) throw std::runtime_error("Invalid boot case count");
        std::uint32_t clears=0,handoffs=0;
        for (std::uint32_t test=0;test<cases;test++) {
            auto ram=std::make_unique<tecmo98::WorkRam>();
            const auto kind=number(input);
            if (kind==1) {
                if (number(input)!=2) throw std::runtime_error("Wrong boot span count");
                std::vector<std::uint8_t> expectedRam,video,expectedVideo;
                for (unsigned i=0;i<2;i++) {
                    const auto address=number(input),length=number(input);
                    if (address!=(i==0?tecmo98::WorkRam::base:tecmo98::video_storage_base) ||
                        length!=(i==0?tecmo98::WorkRam::size:tecmo98::video_storage_size)) throw std::runtime_error("Wrong boot span");
                    auto before=readBytes(input,length),after=readBytes(input,length);
                    if (i==0) {std::copy(before.begin(),before.end(),ram->bytes.begin());expectedRam=std::move(after);}
                    else {video=std::move(before);expectedVideo=std::move(after);}
                }
                tecmo98::clear_06004002(*ram,video);
                if (!std::equal(expectedRam.begin(),expectedRam.end(),ram->bytes.begin()) || video!=expectedVideo)
                    throw std::runtime_error("Original/native boot clear mismatch");
                ++clears;
            } else if (kind==2) {
                const auto entry=number(input),originalStack=number(input);
                ram->write32(0x06064e7c,originalStack);
                const auto native=tecmo98::handoff_060540a4(*ram,entry);
                const std::array<std::uint32_t,6> actual={native.entry,native.originalStack,native.gbr,
                    native.originalStatus,native.controlAddress,native.controlValue};
                for (auto field:actual) if (field!=number(input)) throw std::runtime_error("Original/native trampoline mismatch");
                ++handoffs;
            } else throw std::runtime_error("Unknown boot case");
        }
        if (input.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("Unexpected boot trailer");
        std::cout << "{\"passed\":true,\"clear_cases\":" << clears << ",\"handoff_cases\":" << handoffs
                  << ",\"execution\":\"native Windows x64\",\"playable_game\":false}\n";
        return 0;
    } catch (const std::exception& error) {std::cerr << error.what() << '\n';return 1;}
}
