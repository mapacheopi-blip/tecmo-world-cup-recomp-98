#include "image_loader.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {
void checked(NTSTATUS status) { if (status < 0) throw std::runtime_error("Windows SHA-1 provider failed"); }
struct Algorithm {
    BCRYPT_ALG_HANDLE handle = nullptr;
    ~Algorithm() { if (handle) BCryptCloseAlgorithmProvider(handle, 0); }
};
struct Hash {
    BCRYPT_HASH_HANDLE handle = nullptr;
    ~Hash() { if (handle) BCryptDestroyHash(handle); }
};
std::string sha1(std::vector<std::uint8_t>& data) {
    Algorithm algorithm;
    checked(BCryptOpenAlgorithmProvider(&algorithm.handle, BCRYPT_SHA1_ALGORITHM, nullptr, 0));
    ULONG objectSize = 0, returned = 0;
    checked(BCryptGetProperty(algorithm.handle, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &returned, 0));
    std::vector<std::uint8_t> object(objectSize);
    std::array<std::uint8_t, 20> digest{};
    Hash hash;
    checked(BCryptCreateHash(algorithm.handle, &hash.handle, object.data(), objectSize, nullptr, 0, 0));
    checked(BCryptHashData(hash.handle, data.data(), static_cast<ULONG>(data.size()), 0));
    checked(BCryptFinishHash(hash.handle, digest.data(), static_cast<ULONG>(digest.size()), 0));
    std::ostringstream hex;
    for (auto byte : digest) hex << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
    return hex.str();
}
}

namespace tecmo98 {
std::vector<std::uint8_t> load_resident_profile(const std::filesystem::path& capture) {
    const auto size=std::filesystem::file_size(capture);
    if(size!=0x4000&&size!=WorkRam::size)throw std::runtime_error("Expected 16 KiB resident firmware or verified legacy profile");
    std::ifstream input(capture,std::ios::binary);std::vector<std::uint8_t> bytes(size);
    if(!input.read(reinterpret_cast<char*>(bytes.data()),bytes.size())||input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Invalid resident firmware size");
    const auto expected=size==0x4000?"b985e6bd21be05a0b5187150498e0ce7aa93b13e":"69538b162c56a1075007ab392630070fd067683a";
    if(sha1(bytes)!=expected)throw std::runtime_error("Resident firmware hash mismatch");
    return bytes;
}
CartridgeImage load_cartridge(const std::filesystem::path& mappedCart) {
    std::ifstream input(mappedCart,std::ios::binary);
    std::vector<std::uint8_t> bytes(CartridgeImage::size);
    if (!input.read(reinterpret_cast<char*>(bytes.data()),bytes.size()) || input.peek()!=std::char_traits<char>::eof())
        throw std::runtime_error("Expected verified 48 MiB mapped cartridge");
    // Derived from all five verified chips using scripts/build_cart.py; zero fill and aliases
    // included. The Python verifier independently reconstructs this image from the user's ZIP.
    if (sha1(bytes)!="2e4f293a0b6cc5b32694671cd4d9ea20381add88") throw std::runtime_error("Cartridge SHA-1 mismatch");
    return CartridgeImage(std::move(bytes));
}
FirmwareData load_jp1_bios(const std::filesystem::path& rawBios) {
    std::ifstream input(rawBios,std::ios::binary);
    std::vector<std::uint8_t> bytes(0x80000);
    if (!input.read(reinterpret_cast<char*>(bytes.data()),bytes.size()) || input.peek()!=std::char_traits<char>::eof())
        throw std::runtime_error("Expected unmodified 512 KiB jp1 BIOS");
    if (sha1(bytes)!="eff0f54c70bce05ff3a289bf30b1027e1c8cd117")
        throw std::runtime_error("BIOS SHA-1 mismatch");
    for (std::size_t at=0;at<bytes.size();at+=2) std::swap(bytes[at],bytes[at+1]);
    return FirmwareData(std::move(bytes));
}
ProgramLoad load_program(WorkRam& ram, const std::filesystem::path& eprom) {
    std::ifstream input(eprom, std::ios::binary);
    std::vector<std::uint8_t> bytes(0x100000);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size()) || input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Expected the unmodified 1 MiB game EPROM");
    auto digest = sha1(bytes);
    if (digest != "f66cc955181720661a0334fe67fa5750ddf9758b")
        throw std::runtime_error("Game EPROM SHA-1 mismatch; refusing to run translated code");
    // Observed BIOS transfer: 0x3e000 32-bit words; EPROM[0x4000:0xfc000].
    std::copy(bytes.begin() + 0x4000, bytes.begin() + 0xfc000, ram.bytes.begin() + 0x4000);
    return {0x06004000, 0xf8000, std::move(digest)};
}
}
