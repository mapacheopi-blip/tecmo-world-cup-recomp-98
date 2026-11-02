#include "platform.hpp"

namespace tecmo98 {
void NativePlatform::writeAudioPort2(std::uint8_t value) {
    audioPort2 = value;
    if ((value & 0x10) != 0) audio.assertReset();
    else audio.releaseReset();
}

void NativePlatform::sendSystemCommand(std::uint8_t command) {
    if (command != 3) throw std::invalid_argument("Unimplemented system-manager command");
    if (systemCommandBusy) throw std::runtime_error("System-manager command is busy");
    systemCommandBusy = true;
    // SSHOFF: stop dispatch of native translations of slave-side tasks.
    // The native scheduler is synchronous here; original instruction-delay timing is not modeled.
    slave.assertReset();
    lastSystemCommand = command;
    ++completedSystemCommands;
    systemCommandBusy = false;
}

void resident_06000c0a(WorkRam& ram, NativePlatform& platform,
                     std::uint32_t andMask, std::uint32_t orMask) {
    const auto value = (ram.read32(0x06000348) | orMask) & andMask;
    ram.write32(0x06000348, value);
    platform.interruptMask = value; // Native equivalent of the write to 0x25fe00a0.
    platform.scu.words[0xa0 / 4] = value;
}

void resident_060014c0(WorkRam& ram, std::span<const std::uint8_t> source) {
    if (source.size() != 128) throw std::invalid_argument("Vector source must contain 32 words");
    for (std::uint32_t i = 0; i < 32; ++i) {
        const auto at = i * 4;
        const auto value = (std::uint32_t(source[at]) << 24) | (std::uint32_t(source[at + 1]) << 16) |
                           (std::uint32_t(source[at + 2]) << 8) | source[at + 3];
        ram.write32(0x06000a80 + at, value);
    }
    // The original temporarily masks CPU interrupts; there is no asynchronous dispatch yet.
}

void resident_060014a8(WorkRam& ram, std::uint32_t index, std::uint32_t handler) {
    const auto address = 0x06000900u + (index << 2);
    ram.write32(address, handler != 0 ? handler : 0x06002052u);
}

ResetBoundary begin_reset_000034e8(WorkRam& ram, NativePlatform& platform) {
    // Check dependencies before modifying state; an unknown BIOS binding must not be skipped.
    if (ram.read32(0x06000344) != 0x06000c0a)
        throw std::runtime_error("Unresolved BIOS mask-service binding at 0x06000344");
    if (platform.systemCommandBusy) throw std::runtime_error("System-manager command is busy");
    ram.write32(0x06000824, ram.read32(0x06000824) & 0xffffffbfu);
    resident_06000c0a(ram, platform, 0xfffffffeu, 0x7ffe);
    platform.sendSystemCommand(3);
    return {0x00003504, 0x000040c0}; // Next original call, not an assertion that reset finished.
}

void install_observed_jp1_prefix(WorkRam& ram) {
    // Data identities for native service binding only; no original instructions are installed.
    ram.write32(0x06000628, 0x000034c4);
    ram.write32(0x06000344, 0x06000c0a);
    ram.write32(0x06000348, 0xffff7ffe);
    ram.write32(0x06000824, 0x0000024d);
    ram.write32(0x06000340, 0x06000c00);
    ram.write32(0x06000248, 0x00001008);
    ram.write8(0x0600065b, 0);
    ram.write32(0x06000280, 0x060014c0);
    ram.write32(0x06000300, 0x060014a8);
    ram.write32(0x06000618, 0x06000c34);
}

void bios_000040c0(WorkRam& ram, NativePlatform& platform) {
    const auto value = std::uint8_t(ram.read8(0x0600065b) | 0x10u);
    ram.write8(0x0600065b, value);
    platform.writeAudioPort2(value); // The original write is in the RTS delay slot.
}

void bios_000040a8(NativePlatform& platform) {
    platform.cpuDma.writeOperation(platform.cpuDma.operation() & 0xfffffffeu);
}

void resident_06000c00(WorkRam& ram, NativePlatform& platform, std::uint32_t mask) {
    ram.write32(0x06000348, mask);
    platform.interruptMask = mask;
    platform.scu.words[0xa0 / 4] = mask;
}

void bios_000035d8(WorkRam& ram, NativePlatform& platform) {
    if (ram.read32(0x06000340) != 0x06000c00)
        throw std::runtime_error("Unresolved BIOS mask-set binding at 0x06000340");
    auto& config = platform.scu;
    for (std::uint32_t channel = 0; channel < 3; ++channel) {
        const auto base = channel * 8;
        for (std::uint32_t word = 0; word < 5; ++word) config.words[base + word] = 0;
        config.words[base + 5] = 7;
    }
    config.words[0x60 / 4] = 0;
    config.words[0x80 / 4] = 0;
    config.words[0xb0 / 4] = 0x1ff01ff0;
    config.words[0xb4 / 4] = 0x1ff01ff0;
    config.words[0xb8 / 4] = 0x1f;
    config.words[0xa8 / 4] = 1;
    config.words[0x90 / 4] = 0x3ff;
    config.words[0x94 / 4] = 0x1ff;
    config.words[0x98 / 4] = 0;
    config.regionBusControl = (ram.read32(0x06000248) & 8) != 0 ? 0x0c : 0x0d;
    config.originalVectorBase = 0x06000000; // ABI metadata, never a Windows interrupt pointer.
    resident_06000c00(ram, platform, ram.read32(0x06000348));
    config.prepared = true;
}

ResetBoundary continue_reset_00003504(WorkRam& ram, NativePlatform& platform) {
    if (ram.read32(0x06000340) != 0x06000c00)
        throw std::runtime_error("Unresolved BIOS mask-set binding at 0x06000340");
    bios_000040c0(ram, platform);
    bios_000040a8(platform);
    bios_000035d8(ram, platform);
    return {0x00003510, 0x000040ce};
}
}
