#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

namespace tecmo98 {
// Transitional address-based data adapter. No CPU interpreter or instruction dispatch.
// Only the high work RAM touched by the translated routines is provided here.
class WorkRam {
public:
    static constexpr std::uint32_t base = 0x06000000, size = 0x100000;
    std::array<std::uint8_t, size> bytes{};
    std::size_t offset(std::uint32_t address, std::size_t length = 1) const {
        if (address < base || length > size || std::uint64_t(address - base) + length > size)
            throw std::out_of_range("Unimplemented work RAM address");
        return address - base;
    }
    std::uint8_t read8(std::uint32_t address) const { return bytes[offset(address)]; }
    std::uint16_t read16(std::uint32_t address) const {
        const auto i = offset(address, 2);
        return (std::uint16_t(bytes[i]) << 8) | bytes[i + 1];
    }
    std::uint32_t read32(std::uint32_t address) const {
        return (std::uint32_t(read16(address)) << 16) | read16(address + 2);
    }
    void write8(std::uint32_t address, std::uint8_t value) { bytes[offset(address)] = value; }
    void write16(std::uint32_t address, std::uint16_t value) {
        const auto i = offset(address, 2);
        bytes[i] = std::uint8_t(value >> 8); bytes[i + 1] = std::uint8_t(value);
    }
    void write32(std::uint32_t address, std::uint32_t value) {
        offset(address, 4);
        write16(address, std::uint16_t(value >> 16)); write16(address + 2, std::uint16_t(value));
    }
};

// Original entry 0x0605a670, including branch/RTS delay-slot effects.
// The 3x4 identity matrix interpretation is inferred from the stored values.
std::uint32_t sub_0605a670(WorkRam& memory, std::uint32_t gbr);
// Original entry 0x0605ac54. Returns the original sign-extended R0 value.
std::uint32_t sub_0605ac54(WorkRam& memory, std::uint32_t gbr);
// Only the block [0x06004192, 0x060041aa); this is NOT a full game update.
void block_06004192(WorkRam& memory);
}
