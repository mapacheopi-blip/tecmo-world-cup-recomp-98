#pragma once
#include "game_routines.hpp"
#include <span>
#include <vector>

namespace tecmo98 {
class CartridgeImage {
    std::vector<std::uint8_t> bytes_;
public:
    static constexpr std::uint32_t size=0x3000000;
    explicit CartridgeImage(std::vector<std::uint8_t> bytes);
    std::span<const std::uint8_t> span(std::uint32_t cpuAddress,std::size_t length) const;
    std::uint32_t read32(std::uint32_t cpuAddress) const;
};

// Partial ST-V 315-5881 register adapter. Register state is real; encrypted stream reads
// explicitly stop until a decoder is implemented. No fabricated decryption results.
class ProtectionRegisters {
    std::array<std::uint32_t,4> words_{};
public:
    std::uint16_t requestedAddressLow=0,requestedAddressHigh=0,requestedSubkey=0;
    void write16(std::uint32_t address,std::uint16_t value);
    std::uint16_t read16(std::uint32_t address,const CartridgeImage& cart) const;
    const auto& words() const {return words_;}
};
// Original initial game function. Mismatching cartridge takes an untranslated error-UI
// path: fail explicitly before continuing instead of bypassing the comparison.
void sub_06009614(WorkRam& ram,const CartridgeImage& cart,ProtectionRegisters& protection);
}
