#include "game_routines.hpp"

namespace tecmo98 {
std::uint32_t sub_0605a670(WorkRam& memory, std::uint32_t gbr) {
    constexpr std::uint32_t matrix = 0x0605a6dc;
    memory.write8(gbr + 0x20, 0);
    memory.write32(gbr + 0x1c, matrix); // Original BRA delay slot at 0x0605a678.
    // Preserve store order, fixed-width values and the original address-visible state.
    for (auto word : {0u, 4u, 8u, 1u, 5u, 9u, 2u, 6u, 10u, 3u, 7u, 11u})
        memory.write32(matrix + word * 4, word == 0 || word == 5 || word == 10 ? 0x10000 : 0);
    return 1; // Original RTS delay slot at 0x0605a6b6.
}

std::uint32_t sub_0605ac54(WorkRam& memory, std::uint32_t gbr) {
    // MOV.W literal 0x8000 at 0x0605ac6c sign-extends to 0xffff8000.
    const auto word = std::uint16_t(memory.read16(gbr + 0xc0) | 0x8000u);
    memory.write16(gbr + 0xc0, word); // Original RTS delay slot at 0x0605ac5c.
    return 0xffff0000u | word;
}

void block_06004192(WorkRam& memory) {
    memory.write8(0x06076518, std::uint8_t(memory.read8(0x06076518) + 1u));
    memory.write16(0x060764d6, std::uint16_t(memory.read16(0x060764d6) + 1u));
    memory.write32(0x060764d8, memory.read32(0x060764d8) + 1u);
    memory.write8(0x0607651b, std::uint8_t(memory.read8(0x0607651b) + 0x35u));
}
}
