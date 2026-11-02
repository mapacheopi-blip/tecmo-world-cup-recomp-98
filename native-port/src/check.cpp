#include "game_routines.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace {
std::uint32_t number(std::istream& stream) {
    unsigned char b[4];
    if (!stream.read(reinterpret_cast<char*>(b), 4)) throw std::runtime_error("Truncated fixture");
    return std::uint32_t(b[0]) | (std::uint32_t(b[1]) << 8) |
           (std::uint32_t(b[2]) << 16) | (std::uint32_t(b[3]) << 24);
}
std::vector<std::uint8_t> bytes(std::istream& stream, std::uint32_t length) {
    if (length > tecmo98::WorkRam::size) throw std::runtime_error("Fixture span too large");
    std::vector<std::uint8_t> result(length);
    if (!stream.read(reinterpret_cast<char*>(result.data()), length)) throw std::runtime_error("Truncated fixture span");
    return result;
}
struct Span { std::uint32_t address; std::vector<std::uint8_t> before, after; };
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "TECMO WORLD CUP RECOMP 98 - native routine verification\n"
                         "Development tool, not a playable port.\nUsage: tecmo98-native-check.exe <original-vectors.bin>\n";
            return 2;
        }
        std::ifstream input(std::filesystem::path(argv[1]), std::ios::binary);
        char magic[8];
        if (!input.read(magic, 8) || std::string(magic, 8) != "T98VEC01") throw std::runtime_error("Invalid fixture header");
        const auto count = number(input);
        if (count == 0 || count > 10000) throw std::runtime_error("Invalid fixture count");
        std::uint32_t matrix = 0, flags = 0, counters = 0;
        for (std::uint32_t index = 0; index < count; ++index) {
            const auto kind = number(input), gbr = number(input), expectedR0 = number(input), n = number(input);
            if (n == 0 || n > 8) throw std::runtime_error("Invalid span count");
            auto ram = std::make_unique<tecmo98::WorkRam>();
            std::vector<Span> spans;
            for (std::uint32_t i = 0; i < n; ++i) {
                const auto address = number(input), length = number(input);
                const auto offset = ram->offset(address, length);
                Span span{address, bytes(input, length), bytes(input, length)};
                std::copy(span.before.begin(), span.before.end(), ram->bytes.begin() + offset);
                spans.push_back(std::move(span));
            }
            auto expected = std::make_unique<tecmo98::WorkRam>(*ram);
            for (const auto& span : spans)
                std::copy(span.after.begin(), span.after.end(), expected->bytes.begin() + expected->offset(span.address));
            std::uint32_t result = 0;
            switch (kind) {
                case 1: result = tecmo98::sub_0605a670(*ram, gbr); ++matrix; break;
                case 2: result = tecmo98::sub_0605ac54(*ram, gbr); ++flags; break;
                case 3: tecmo98::block_06004192(*ram); ++counters; break;
                default: throw std::runtime_error("Unknown routine");
            }
            if ((kind != 3 && result != expectedR0) || ram->bytes != expected->bytes)
                throw std::runtime_error("Original instruction/native mismatch in case " + std::to_string(index));
        }
        if (input.peek() != std::char_traits<char>::eof()) throw std::runtime_error("Unexpected fixture trailer");
        std::cout << "{\"passed\":true,\"cases\":" << count << ",\"matrix\":" << matrix
                  << ",\"flags\":" << flags << ",\"counter_blocks\":" << counters
                  << ",\"execution\":\"native Windows x64\",\"playable_game\":false}\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
