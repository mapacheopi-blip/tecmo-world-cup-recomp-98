#include "native_scsp.hpp"
#include <functional>
#include <iostream>
#include <string>
#include <algorithm>
using tecmo98::NativeScsp;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::uint64_t tick(unsigned frames){return (std::uint64_t(frames)*1000000000+44099)/44100;}
struct Fixture {
 std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x100000);NativeScsp chip;
 Fixture(){chip.attachRam(ram);chip.write(0x400,15,0);}
 void slot(unsigned start,unsigned key,unsigned lsa,unsigned lea,unsigned pitch=0,unsigned index=0){
  const unsigned base=index*32;chip.write(base+2,start&65535,0);chip.write(base+4,lsa,0);chip.write(base+6,lea,0);chip.write(base+12,0x100,0);chip.write(base+16,pitch,0);chip.write(base+22,0xe000,0);chip.write(base,std::uint16_t(0x1800|key|(start>>16)),0);
 }
 std::vector<std::int16_t> render(unsigned frames){chip.poll(tick(frames));return chip.takeAudio();}
};
int main(){unsigned failures=0,cases=0;
 const auto test=[&](const char* name,const std::function<void()>& body){++cases;try{body();std::cout<<"PASS "<<name<<"\n";}catch(const std::exception& e){++failures;std::cout<<"FAIL "<<name<<": "<<e.what()<<"\n";}};
 test("upper PCM8 bank",[]{Fixture f;f.ram[0x80000]=127;f.ram[0]=1;f.slot(0x80000,0x10,0,8);auto a=f.render(1);require(a[0]==32512&&a[1]==32512,"SA bit 19 aliased to lower bank");});
 test("PCM16 big endian and 20-bit wrap",[]{Fixture f;f.ram[0xffffe]=0x12;f.ram[0xfffff]=0x34;f.ram[0]=0xfe;f.ram[1]=0xdc;f.slot(0xffffe,0,0,8);auto a=f.render(2);require(a==std::vector<std::int16_t>{0x1234,0x1234,-292,-292},"16-bit sample bank/wrap/order differ");});
 test("negative fractional interpolation",[]{Fixture f;f.ram[0]=0xff;f.slot(0,0x10,0,8,0x4040);auto a=f.render(2);require(a[2]==-255,"interpolation must round negative fractions down");});
 test("reverse loop lower-bound wrap",[]{Fixture f;for(unsigned i=0;i<10;++i)f.ram[i]=std::uint8_t((i+1)*10);f.slot(0,0x50,2,5);auto a=f.render(10);unsigned expected[]={0,1,3,2,4,3,2,4,3,2};for(unsigned i=0;i<10;++i)if(a[i*2]!=int((expected[i]+1)*10*256))throw std::runtime_error("frame "+std::to_string(i)+" got "+std::to_string(a[i*2])+" expected "+std::to_string((expected[i]+1)*10*256));});
 test("monitor address units and write-only selector",[]{Fixture f;f.slot(0,0x30,0,65535,0,17);f.chip.write(0x408,17u<<11,0);f.render(1);require(f.chip.read(0x408)==20,"CA used low sample bits or exposed MSLC");f.render(4097);require(f.chip.read(0x408)==0x94,"CA did not advance in 4096-sample blocks");Fixture fractional;fractional.chip.write(8,1,0);fractional.chip.write(10,15u<<10,0);fractional.slot(0,0x30,0,65535);fractional.chip.write(12,0,0);fractional.render(1);require((fractional.chip.read(0x408)&31)==20,"monitor envelope rounded fractional level incorrectly");});
 test("zero decay2 holds with key scaling",[]{Fixture f;std::fill(f.ram.begin(),f.ram.end(),100);f.chip.write(8,31|(31<<6),0);f.chip.write(10,14u<<10,0);f.slot(0,0x30,0,65535,0x3800);f.chip.write(12,0,0);auto a=f.render(1000);if(a[10]!=a[1998]||a[10]<20000)throw std::runtime_error("D2R zero changed "+std::to_string(a[10])+" to "+std::to_string(a[1998]));});
 std::cout<<"{\"cases\":"<<cases<<",\"failures\":"<<failures<<"}\n";return failures?1:0;
}
