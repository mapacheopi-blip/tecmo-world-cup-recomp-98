#include "native_pixel_filter.hpp"
#include "reference/pixel_filter_v041.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
void require(bool v){if(!v)throw std::runtime_error("Scale2x differs from preserved scalar output");}
void compare(const tecmo98::NativeFrame& f){tecmo98::NativeFrame a,b;tecmo98::scale2x_reference(f,a);tecmo98::scale2x(f,b);require(a.width==b.width&&a.height==b.height&&a.pixels==b.pixels);}
volatile std::uint32_t sink=0;
template<class F> double bench(const tecmo98::NativeFrame& f,F filter){tecmo98::NativeFrame out;filter(f,out);const auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<50;++i){filter(f,out);sink=out.pixels[i%out.pixels.size()];}return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/50;}
}
int main(int argc,char** argv){try{
 if(argc>2)throw std::runtime_error("pixel-filter-check [snapshot directory]");
 unsigned cases=0;std::uint32_t random=1789;
 for(unsigned w=1;w<=35;++w)for(unsigned h=1;h<=35;++h)for(unsigned pattern=0;pattern<6;++pattern){
  tecmo98::NativeFrame f{w,h,std::vector<std::uint32_t>(w*h)};
  for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){random=random*1664525+1013904223;f.pixels[y*w+x]=pattern==0?0:pattern==1?0xffffffff:pattern==2?(x+y)%2:pattern==3?unsigned(x==y):pattern==4?random%5:random;}
  compare(f);++cases;
 }
 tecmo98::NativeFrame f;
 if(argc==2){std::array<std::vector<std::uint8_t>,6> bytes;unsigned i=0;
  for(const auto& [name,size]:std::array<std::pair<const char*,unsigned>,6>{{{"native-display.bin",0x40000},{"native-draw.bin",0x40000},{"native-vdp2.bin",0x80000},{"native-cram.bin",0x1000},{"native-vdp1-regs.bin",32},{"native-vdp2-regs.bin",512}}}){auto& b=bytes[i++];b.resize(size);std::ifstream in(std::filesystem::path(argv[1])/name,std::ios::binary);if(!in.read(reinterpret_cast<char*>(b.data()),size)||in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Invalid snapshot");}
  f=tecmo98::compose_native_frame({bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5]});
 }else{f={704,448,std::vector<std::uint32_t>(704*448)};for(auto& p:f.pixels){random=random*1664525+1013904223;p=random%7;}}
 compare(f);++cases;
 std::array<double,9> old{},current{};for(unsigned i=0;i<9;++i){if(i%2){old[i]=bench(f,tecmo98::scale2x_reference);current[i]=bench(f,tecmo98::scale2x);}else{current[i]=bench(f,tecmo98::scale2x);old[i]=bench(f,tecmo98::scale2x_reference);}}
 std::sort(old.begin(),old.end());std::sort(current.begin(),current.end());
 std::cout<<"{\"passed\":true,\"byte_exact_cases\":"<<cases<<",\"width\":"<<f.width<<",\"height\":"<<f.height<<",\"scalar_ms\":"<<old[4]<<",\"optimized_ms\":"<<current[4]<<",\"speedup\":"<<old[4]/current[4]<<",\"source\":\""<<(argc==2?"game snapshot":"synthetic")<<"\"}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
