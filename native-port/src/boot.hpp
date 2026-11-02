#pragma once
#include "game_routines.hpp"
#include <span>

namespace tecmo98 {
constexpr std::uint32_t video_storage_base = 0x05c00000;
constexpr std::uint32_t video_storage_size = 0x000c0000;
constexpr std::uint32_t startup_clear_begin = 0x060764d0;

// Original [0x06004002,0x0600402a). Clears data storage, not GPU behavior.
// The SH-2 ABI's preceding PR save is outside this translated block.
void clear_06004002(WorkRam& ram, std::span<std::uint8_t> videoStorage);

struct BootHandoff {
    std::uint32_t entry, originalStack, gbr, originalStatus;
    std::uint32_t controlAddress;
    std::uint8_t controlValue;
};
// Data contract of the original trampoline, not a CPU context/interpreter.
// The native caller uses gbr as an explicit argument and its own Windows stack.
BootHandoff handoff_060540a4(const WorkRam& ram, std::uint32_t entry);
}
