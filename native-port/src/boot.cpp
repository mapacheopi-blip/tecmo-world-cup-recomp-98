#include "boot.hpp"
#include <algorithm>

namespace tecmo98 {
void clear_06004002(WorkRam& ram, std::span<std::uint8_t> videoStorage) {
    if (videoStorage.size() != video_storage_size)
        throw std::invalid_argument("Startup video data storage must be exactly 0xc0000 bytes");
    std::fill(ram.bytes.begin() + ram.offset(startup_clear_begin), ram.bytes.end(), 0);
    std::fill(videoStorage.begin(), videoStorage.end(), 0);
}

BootHandoff handoff_060540a4(const WorkRam& ram, std::uint32_t entry) {
    // Literal 0x060540bc points to stack-pointer storage, not GBR.
    // Literal 0x060540c0 is loaded into GBR in the JMP delay slot.
    return {entry, ram.read32(0x06064e7c), 0x060ffc00, 0xf0, 0xfffffe92, 0x11};
}
}
