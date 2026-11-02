#include "recompiled.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
class Ram:public tecmo98::aot::DataBus {
public:std::vector<std::uint8_t> bytes;bool loopFixture=false;unsigned sleeps=0;
 // Isolated loop fixture: no join input, and deterministic one-frame yields.
 // These stubs exist only in this check, never in the native game.
 bool runNativeFunction(tecmo98::aot::Context& c)override{
  if(!loopFixture)return false;
  if(c.pc==0x06008ba0){c.reg(0,0);c.pc=c.special(0x114);--c.remaining;++c.executed;return true;}
  if(c.pc==0x06005634){if(c.reg(4)!=1)throw std::runtime_error("Unexpected wait request");++sleeps;c.pc=c.special(0x114);--c.remaining;++c.executed;return true;}
  return false;
 }
 Ram(const char* path):bytes(0x100000){std::ifstream f(path,std::ios::binary);if(!f.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))throw std::runtime_error("Invalid captured RAM");}
 std::uint64_t read(std::uint32_t a,unsigned n)override{a&=~0x20000000u;if(a<0x6000000||std::uint64_t(a)+n>0x6100000)throw std::runtime_error("Unexpected RAM read");std::uint64_t v=0;for(unsigned i=0;i<n;++i)v=(v<<8)|bytes[a-0x6000000+i];return v;}
 void write(std::uint32_t a,unsigned n,std::uint64_t v)override{a&=~0x20000000u;if(a<0x6000000||std::uint64_t(a)+n>0x6100000)throw std::runtime_error("Unexpected RAM write");for(unsigned i=0;i<n;++i)bytes[a-0x6000000+i]=std::uint8_t(v>>(8*(n-i-1)));}
 void verifySource(std::uint32_t a,std::uint32_t v,unsigned n)override{if(read(a,n)!=v)throw std::runtime_error("Original source guard mismatch");}
};
int main(int argc,char** argv){if(argc!=2)return 2;try{unsigned failures=0,cases=0;
 for(auto start:{0x6008328u,0x6008370u})for(unsigned frames:{0u,1u,29u,30u,31u,209u,210u,211u,65535u}){
  Ram ram(argv[1]);tecmo98::aot::Context c;c.pc=start;c.stop=start==0x6008328?0x600833e:0x6008386;c.reg(10,frames);c.reg(15,0x060c7bb4);
  tecmo98::aot::run(c,ram,5000);if(c.pc!=c.stop)throw std::runtime_error("Timer comparison did not return");
  const auto limit=start==0x6008328?30u:210u,expected=unsigned(frames<limit);
  ++cases;bool ok=c.reg(0)==expected&&c.reg(15)==0x060c7bb4;if(!ok)++failures;
  std::cout<<"frames="<<frames<<" limit="<<limit<<" result="<<c.reg(0)<<" expected="<<expected<<" pass="<<ok<<"\n";
 }
 for(auto start:{0x6008328u,0x6008344u}){
  Ram ram(argv[1]);ram.loopFixture=true;tecmo98::aot::Context c;c.pc=start;c.stop=start==0x6008328?0x6008342:0x600838a;c.reg(9,0);c.reg(10,0);c.reg(14,0x6005634);c.reg(15,0x060c7bb4);
  tecmo98::aot::run(c,ram,1000000);const auto expected=start==0x6008328?30u:210u;
  if(c.pc!=c.stop||ram.sleeps!=expected||c.reg(10)!=expected)throw std::runtime_error("Original round wait loop did not terminate on its deadline");
 }
 bool rejected=false;{Ram ram(argv[1]);ram.bytes[0x8330]^=1;tecmo98::aot::Context c;c.pc=0x6008328;c.stop=0x600833e;c.reg(10,31);c.reg(15,0x60c7bb4);try{tecmo98::aot::run(c,ram,5000);}catch(const std::exception&){rejected=true;}}
 if(!rejected)throw std::runtime_error("Changed original opcode accepted");
 std::cout<<"{\"cases\":"<<cases<<",\"failures\":"<<failures<<",\"translated_wait_loops\":2,\"source_mutation_rejected\":true}\n";return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
