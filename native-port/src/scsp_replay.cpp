// Offline external-reference trace replay; never used by the game runtime.
#include "native_scsp.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
int main(int argc,char** argv){try{
 if(argc!=5)throw std::runtime_error("<initial RAM> <events.bin> <output.wav> <seconds>");
 std::vector<std::uint8_t> ram(0x100000);std::ifstream initial(argv[1],std::ios::binary);
 if(!initial.read(reinterpret_cast<char*>(ram.data()),ram.size()))throw std::runtime_error("Invalid initial RAM");
 tecmo98::NativeScsp scsp;scsp.attachRam(ram);scsp.poll(0);
 std::ifstream events(argv[2],std::ios::binary);std::ofstream out(argv[3],std::ios::binary);
 if(!events||!out)throw std::runtime_error("Cannot open replay input/output");
 const auto le=[&](unsigned x,unsigned bytes){for(unsigned i=0;i<bytes;++i)out.put(char(x>>(8*i)));};
 out.write("RIFF",4);le(0,4);out.write("WAVEfmt ",8);le(16,4);le(1,2);le(2,2);le(44100,4);le(176400,4);le(4,2);le(16,2);out.write("data",4);le(0,4);
 std::uint64_t samples=0,count=0,previous=0,backtracks=0,maxBacktrack=0;std::array<char,20> record{};
 auto drain=[&]{const auto audio=scsp.takeAudio();out.write(reinterpret_cast<const char*>(audio.data()),audio.size()*2);samples+=audio.size();};
 while(events.read(record.data(),record.size())){
  double seconds;std::uint32_t address,data,mask;std::memcpy(&seconds,record.data(),8);std::memcpy(&address,record.data()+8,4);std::memcpy(&data,record.data()+12,4);std::memcpy(&mask,record.data()+16,4);
  if(!std::isfinite(seconds)||seconds<0||seconds>600)throw std::runtime_error("Invalid event timestamp");
  // MAME CPU-local tap times can overlap; its sound stream cannot rewind.
  const auto rawNs=std::uint64_t(std::llround(seconds*1e9));if(rawNs<previous){if(previous-rawNs>100000)throw std::runtime_error("Reference timestamp backtrack exceeds 100 microseconds");++backtracks;maxBacktrack=std::max(maxBacktrack,previous-rawNs);}const auto ns=std::max(previous,rawNs);previous=ns;
  scsp.poll(ns);drain();
  if(address>=0x05a00000&&address<=0x05affffc){for(unsigned b=0;b<4;++b){const auto shift=24-8*b,at=address-0x05a00000+b;ram[at]=std::uint8_t((ram[at]&~(mask>>shift))|((data&mask)>>shift));}}
  else if(address>=0x100000&&address<0x101000){const auto a=address-0x100000;const auto value=std::uint16_t((scsp.read(a)&~mask)|(data&mask));scsp.write(a,value,ns);}
  else throw std::runtime_error("Unexpected trace address");
  ++count;
 }
 if(!events.eof()||events.gcount()!=0)throw std::runtime_error("Truncated trace");
 scsp.poll(std::max(previous,std::uint64_t(std::llround(std::stod(argv[4])*1e9))));drain();
 if(!out)throw std::runtime_error("Failed to write replay PCM");
 out.seekp(4);le(36+unsigned(samples*2),4);out.seekp(40);le(unsigned(samples*2),4);
 std::cout<<"{\"events\":"<<count<<",\"frames\":"<<samples/2<<",\"timestamp_backtracks\":"<<backtracks<<",\"max_backtrack_ns\":"<<maxBacktrack<<"}\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
