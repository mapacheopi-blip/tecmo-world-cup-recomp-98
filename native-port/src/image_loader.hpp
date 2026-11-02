#pragma once
#include "game_routines.hpp"
#include "reset_tail.hpp"
#include "cartridge.hpp"
#include <filesystem>
#include <string>
namespace tecmo98 {
struct ProgramLoad { std::uint32_t address, bytes; std::string epromSha1; };
// Input: the user's verified, unmodified 1 MiB boot EPROM (not a ZIP or BIOS).
ProgramLoad load_program(WorkRam& ram, const std::filesystem::path& eprom);
FirmwareData load_jp1_bios(const std::filesystem::path& rawBios);
CartridgeImage load_cartridge(const std::filesystem::path& mappedCart);
std::vector<std::uint8_t> load_resident_profile(const std::filesystem::path& capture);
}
