#pragma once
#include "platform.hpp"
#include <vector>

namespace tecmo98 {
// Data view of the verified, byte-swapped jp1 firmware. Never executed by the product.
class FirmwareData {
    std::vector<std::uint8_t> bytes_;
public:
    explicit FirmwareData(std::vector<std::uint8_t> bytes);
    std::span<const std::uint8_t> span(std::uint32_t address, std::size_t size) const;
    std::uint32_t read32(std::uint32_t address) const;
};

struct CacheWrite {
    std::uint32_t address, value, size;
    bool operator==(const CacheWrite&) const = default;
};
struct NativeCodePolicy {
    // Native code reads one coherent WorkRam. No SH-2 cache, cached instructions or JIT.
    // Retain the original requests as an audit; do not claim cache timing/readback fidelity.
    std::vector<CacheWrite> resetRequests;
    std::uint64_t invalidationGeneration = 0;
    std::uint8_t originalControl = 0;
};
struct OnChipResetConfiguration {
    // Image of requested writes, not a model of timer/serial/DMA interrupt delivery.
    std::array<std::uint8_t,0x200> requested{};
    std::uint32_t originalGbr = 0;
    bool prepared = false;
};
struct DisplayResetState {
    std::uint16_t control = 0, status = 0;
};
struct DisplayResetResult { bool completed; std::uint32_t originalResult; };

void bios_000040ce(NativeCodePolicy& code);
std::uint32_t bios_00003020(OnChipResetConfiguration& config, std::uint32_t argument);
// Ordered word copy; reads descriptors/source from BIOS data or WorkRam. Checks all bounds first.
std::uint32_t bios_00000d10(WorkRam& ram, const FirmwareData& bios, std::uint32_t descriptor);
void continue_reset_00003510(WorkRam& ram, const FirmwareData& bios,
                            NativeCodePolicy& code, OnChipResetConfiguration& onChip);
// Final [0x34ca,0x34e0) display phase. Yield instead of blocking Windows in a polling loop.
DisplayResetResult finish_display_000034ca(DisplayResetState& display);
}
