#pragma once
#include "game_routines.hpp"
#include <functional>
#include <span>

namespace tecmo98 {
class TaskResetGate {
    bool resetHeld_ = false;
public:
    bool resetHeld() const { return resetHeld_; }
    void assertReset() { resetHeld_ = true; }
    void releaseReset() { resetHeld_ = false; }
    bool tryRun(const std::function<void()>& task) const {
        if (resetHeld_) return false;
        task(); return true;
    }
};

class CpuDmaDispatchGate {
    // Global DME gate only. Channel selection, transfers and their timing remain unimplemented.
    std::uint8_t operation_;
public:
    explicit CpuDmaDispatchGate(std::uint8_t initial = 0) : operation_(initial & 0x0f) {}
    std::uint32_t operation() const { return operation_; }
    void writeOperation(std::uint32_t value) {
        // DMAOR error flags can be cleared by software, but not raised by a register write.
        operation_ = std::uint8_t(((value & ~6u) | (operation_ & value & 6u)) & 0x0f);
    }
    bool tryDispatch(const std::function<void()>& task) const {
        if ((operation_ & 1) == 0) return false;
        task(); return true;
    }
};

struct ScuResetConfiguration {
    // Written configuration image; transfer engines and scanline timing are not implemented here.
    std::array<std::uint32_t,48> words{};
    std::uint16_t regionBusControl = 0;
    std::uint32_t originalVectorBase = 0;
    bool prepared = false;
};

class NativeBusConfiguration {
    // BCR1/BCR2/WCR/MCR observable configuration, not host memory wait-state timing.
    std::array<std::uint32_t,4> values_{};
public:
    std::uint32_t read(unsigned index) const {
        if(index>=values_.size())throw std::out_of_range("Unknown native bus configuration");
        return values_[index]&(index==0?~0xe008u:index==3?~0x103u:0xffffffffu);
    }
    void write(unsigned index,std::uint32_t value) {
        if(index>=values_.size())throw std::out_of_range("Unknown native bus configuration");
        if(index<2){if((value&0xffff0000)==0xa55a0000)values_[index]=value&0xffff;}
        else values_[index]=value;
    }
};

struct NativePlatform {
    std::uint32_t interruptMask = 0;
    TaskResetGate slave, audio;
    CpuDmaDispatchGate cpuDma;
    ScuResetConfiguration scu;
    std::uint8_t audioPort2 = 0;
    std::uint8_t lastSystemCommand = 0;
    std::uint32_t completedSystemCommands = 0;
    bool systemCommandBusy = false;
    // Only the command identified in this reset path is supported. No generic success stubs.
    void sendSystemCommand(std::uint8_t command);
    void writeAudioPort2(std::uint8_t value);
};

// Translated RAM-resident BIOS services. Their addresses are provenance, not native pointers.
void resident_06000c0a(WorkRam& ram, NativePlatform& platform,
                     std::uint32_t andMask, std::uint32_t orMask);
// Deliberately reads each word before writing it: preserves original forward-copy aliasing.
void resident_060014c0(WorkRam& ram, std::span<const std::uint8_t> source);
void resident_060014a8(WorkRam& ram, std::uint32_t index, std::uint32_t handler);

struct ResetBoundary { std::uint32_t resumeAddress, nextService; };
// Original BIOS [0x34e8,0x3504), including its actual mask service and SMPC command.
// Caller must provide the selected firmware's data bindings; unknown bindings stop here.
ResetBoundary begin_reset_000034e8(WorkRam& ram, NativePlatform& platform);

// Explicit partial native ABI setup from the observed jp1 entry state. Not full firmware state.
void install_observed_jp1_prefix(WorkRam& ram);

void bios_000040c0(WorkRam& ram, NativePlatform& platform);
void bios_000040a8(NativePlatform& platform);
void resident_06000c00(WorkRam& ram, NativePlatform& platform, std::uint32_t mask);
// BIOS 0x35d8 without SH-2 ABI stack saves/restores; includes helper 0x3646 and mask service.
void bios_000035d8(WorkRam& ram, NativePlatform& platform);
// Continue only after begin_reset_000034e8: audio reset, CPU DMA disable, SCU configuration.
ResetBoundary continue_reset_00003504(WorkRam& ram, NativePlatform& platform);
}
