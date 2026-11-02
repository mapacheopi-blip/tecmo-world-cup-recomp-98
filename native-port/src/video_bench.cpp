
#include "native_video.hpp"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){try{
 if(argc<2||argc>3)throw std::runtime_error("video-bench <snapshot directory> [iterations]");
 const unsigned iterations=argc==3?std::stoul(argv[2]):30;if(iterations==0||iterations>1000)throw std::runtime_error("Invalid iterations");
 std::array<std::vector<std::uint8_t>,6> b;unsigned i=0;
 for(const auto& [name,size]:std::array<std::pair<const char*,unsigned>,6>{{{"native-display.bin",0x40000},{"native-draw.bin",0x40000},{"native-vdp2.bin",0x80000},{"native-cram.bin",0x1000},{"native-vdp1-regs.bin",32},{"native-vdp2-regs.bin",512}}}){auto& v=b[i++];v.resize(size);std::ifstream in(std::filesystem::path(argv[1])/name,std::ios::binary);if(!in.read(reinterpret_cast<char*>(v.data()),size)||in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Invalid snapshot");}
 const tecmo98::NativeVideoView view{b[0],b[1],b[2],b[3],b[4],b[5]};auto frame=tecmo98::compose_native_frame(view);const auto start=std::chrono::steady_clock::now();
 for(unsigned n=0;n<iterations;++n)frame=tecmo98::compose_native_frame(view);
 const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/iterations;
 std::uint64_t hash=14695981039346656037ull;for(auto pixel:frame.pixels)for(unsigned shift=0;shift<32;shift+=8){hash^=(pixel>>shift)&255;hash*=1099511628211ull;}
 std::cout<<"{\"iterations\":"<<iterations<<",\"ms_per_frame\":"<<ms<<",\"width\":"<<frame.width<<",\"height\":"<<frame.height<<",\"pixels_fnv1a64\":\""<<std::hex<<hash<<"\"}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
