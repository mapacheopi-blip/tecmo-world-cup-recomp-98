#include "cartridge.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

namespace {
std::uint32_t u32(std::istream& in) {
    unsigned char b[4];if (!in.read(reinterpret_cast<char*>(b),4)) throw std::runtime_error("Truncated cartridge fixture");
    return b[0]|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
template<std::size_t N> std::array<std::uint8_t,N> bytes(std::istream& in) {
    std::array<std::uint8_t,N> b{};if (!in.read(reinterpret_cast<char*>(b.data()),N)) throw std::runtime_error("Truncated cartridge bytes");return b;
}
void contracts() {
    std::vector<std::uint8_t> image(tecmo98::CartridgeImage::size);image[0x2fffffc]=0x12;image[0x2fffffd]=0x34;
    tecmo98::CartridgeImage cart(std::move(image));tecmo98::ProtectionRegisters protection;
    if (protection.read16(0x04fffffc,cart)!=0x1234) throw std::runtime_error("Disabled protection lost ROM fallback");
    protection.write16(0x04fffff8,0x1234);protection.write16(0x04fffffa,0xabcd);
    if (protection.read16(0x04fffff8,cart)!=0x1234 || protection.read16(0x04fffffa,cart)!=0xabcd || protection.requestedAddressLow!=0x1234 || protection.requestedAddressHigh!=0xabcd)
        throw std::runtime_error("Protection word lanes/address requests failed");
    protection.write16(0x04fffffc,0x5678);protection.write16(0x04fffffe,0x1234);
    if (protection.requestedSubkey!=0x5678) throw std::runtime_error("Protection subkey request changed from lower lane");
    protection.write16(0x04fffff0,1);
    bool refused=false;try {protection.read16(0x04fffffc,cart);} catch (const std::runtime_error&) {refused=true;}
    if (!refused) throw std::runtime_error("Unimplemented encrypted stream was accepted");
    if (cart.read32(0x04fffffc)!=cart.read32(0x24fffffc)) throw std::runtime_error("Cartridge alias did not share data");
    refused=false;try {cart.span(0x05000000,1);} catch (const std::out_of_range&) {refused=true;}
    if (!refused) throw std::runtime_error("Cartridge upper bound was accepted");
}
}
int main(int argc,char** argv) {
    try {
        if (argc!=2) {std::cerr<<"Usage: tecmo98-native-cartridge-check <cartridge-vectors.bin>\n";return 2;}
        std::ifstream in(std::filesystem::path(argv[1]),std::ios::binary);char magic[8];
        if (!in.read(magic,8) || std::string(magic,8)!="T98CART5") throw std::runtime_error("Wrong cartridge fixture");
        const auto count=u32(in);if (count==0 || count>10000) throw std::runtime_error("Invalid cartridge case count");
        unsigned happy=0,mismatches=0;
        for (unsigned test=0;test<count;++test) {
            const auto matches=u32(in),flag=u32(in),expectedFlag=u32(in);
            const auto signature=bytes<14>(in),expectedSignature=bytes<14>(in);
            const auto beforeRegisters=bytes<16>(in),afterRegisters=bytes<16>(in);
            auto ram=std::make_unique<tecmo98::WorkRam>();ram->bytes.fill(0xa5);
            for (unsigned i=0;i<14;++i) ram->write8(0x06065120+i,expectedSignature[i]);
            ram->write8(0x0607931c,std::uint8_t(flag));
            auto expectedRam=std::make_unique<tecmo98::WorkRam>(*ram);expectedRam->write8(0x0607931c,std::uint8_t(expectedFlag));
            std::vector<std::uint8_t> image(tecmo98::CartridgeImage::size);
            std::copy(signature.begin(),signature.end(),image.begin()+0x400000);
            tecmo98::CartridgeImage cart(std::move(image));tecmo98::ProtectionRegisters protection;
            for (unsigned i=0;i<16;i+=2) protection.write16(0x04fffff0+i,std::uint16_t((beforeRegisters[i]<<8)|beforeRegisters[i+1]));
            bool completed=true;
            try {tecmo98::sub_06009614(*ram,cart,protection);} catch (const std::runtime_error& error) {
                if (std::string(error.what()).find("untranslated original error-UI path")==std::string::npos) throw;
                completed=false;
            }
            if (completed!=(matches==1) || ram->bytes!=expectedRam->bytes) throw std::runtime_error("Cartridge routine mismatch at case "+std::to_string(test));
            for (unsigned i=0;i<16;++i)
                if (std::uint8_t(protection.words()[i/4]>>(24-8*(i%4)))!=afterRegisters[i]) throw std::runtime_error("Protection register mismatch");
            if (completed) ++happy;else ++mismatches;
        }
        if (in.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("Unexpected cartridge trailer");
        contracts();
        std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"matching_signature_cases\":"<<happy<<",\"mismatching_signature_cases\":"<<mismatches
                 <<",\"encrypted_stream_rejected\":true,\"word_lanes_checked\":true,\"playable_game\":false}\n";
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
