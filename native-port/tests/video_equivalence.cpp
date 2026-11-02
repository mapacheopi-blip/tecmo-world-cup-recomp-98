
#include "native_frame_composer.hpp"
#include "native_pixel_filter.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
namespace tecmo98 {NativeFrame compose_native_frame_reference(const NativeVideoView&);}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("video-equivalence <snapshot directory>");
 std::array<std::vector<std::uint8_t>,6> original;unsigned n=0;
 for(const auto& [name,size]:std::array<std::pair<const char*,unsigned>,6>{{{"native-display.bin",0x40000},{"native-draw.bin",0x40000},{"native-vdp2.bin",0x80000},{"native-cram.bin",0x1000},{"native-vdp1-regs.bin",32},{"native-vdp2-regs.bin",512}}}){auto& b=original[n++];b.resize(size);std::ifstream in(std::filesystem::path(argv[1])/name,std::ios::binary);if(!in.read(reinterpret_cast<char*>(b.data()),size))throw std::runtime_error("Invalid snapshot");}
 const auto view=[](const auto& b){return tecmo98::NativeVideoView{b[0],b[1],b[2],b[3],b[4],b[5]};};
 const auto word=[](const auto& b,unsigned a){return (unsigned(b[a])<<8)|b[a+1];};
 const auto set=[](auto& b,unsigned a,unsigned v){b[a]=v>>8;b[a+1]=v;};
 const auto set32=[&](auto& b,unsigned a,unsigned v){set(b,a,v>>16);set(b,a+2,v);};
 std::mt19937 rng(1998);unsigned cases=0;
 for(unsigned test=0;test<64;++test){auto b=original;auto& r=b[5];auto& ram=b[2];
  if(test){const auto mode=std::array<unsigned,8>{0x8000,0x8001,0x8002,0x8003,0x80c0,0x80c1,0x80c2,0x81c3}[test%8];set(r,0,mode);set(r,0xe,(word(r,0xe)&~0x3000u)|((test%3)<<12));
   set(r,0x110,test&1?0x7f:0);for(unsigned i=0;i<6;++i)set(r,0x114+i*2,rng()&511);set(r,0x112,rng()&127);
   const auto windows=std::array<unsigned,4>{0,3,0x8f,0x0b}[test%4];set(r,0xd4,windows|(windows<<8));
   set(r,0xc0,test%3);set(r,0xc2,test%5);set(r,0xc4,test%2?703:704);set(r,0xc6,test%2?447:448);
   set(r,0xc8,16);set(r,0xca,20);set(r,0xcc,150);set(r,0xce,190);
   const auto table=((((word(r,0xbc)&7)<<16)|word(r,0xbe))*2)&~128u;
   for(unsigned p=0;p<2;++p){const auto a=(table+p*128)&0x7ffff;set32(ram,a+23*4,test%4==0?0:test%4==1?65536:test%4==2?0xffff0000u:16384);
    set32(ram,a+5*4,test%2?65536:49152);set32(ram,a+6*4,test%3?0:0xffff0000u);}
   set(r,0xb0,test%3);if(test%7==0)set(r,0x20,word(r,0x20)&~0x10u);if(test%11==0)set(r,0x20,0x10);
   if(test%5==0)set(b[4],0,0);for(unsigned i=0;i<64;++i)b[3][rng()%4096]=rng();
  }
  const auto before=tecmo98::compose_native_frame_reference(view(b)),after=tecmo98::compose_native_frame(view(b));
  if(before.width!=after.width||before.height!=after.height||before.pixels!=after.pixels)throw std::runtime_error("Pixel mismatch in compositor case "+std::to_string(test));++cases;
 }
 tecmo98::NativeFrameComposer worker;auto b=original;const auto expected=tecmo98::compose_native_frame(view(b));const auto seq=worker.submit(view(b),1234);for(auto& data:b)std::fill(data.begin(),data.end(),0);
 auto result=worker.waitFor(seq,std::chrono::seconds(3));if(!result||!result->error.empty()||result->time!=1234||result->frame.pixels!=expected.pixels)throw std::runtime_error("Worker read mutable session memory");
 b=original;set(b[5],0x20,0x20);const auto bad=worker.submit(view(b),1235);result=worker.waitFor(bad,std::chrono::seconds(3));if(!result||result->error.empty())throw std::runtime_error("Worker hid an unsupported video error");
 b=original;std::uint64_t last=0;for(unsigned i=0;i<30;++i)last=worker.submit(view(b),2000+i);result=worker.waitFor(last,std::chrono::seconds(3));if(!result||!result->error.empty()||result->sequence!=last||result->frame.pixels!=expected.pixels)throw std::runtime_error("Worker did not recover/publish latest snapshot");
 tecmo98::NativeFrameComposer filteredWorker(2);const auto filteredSeq=filteredWorker.submit(view(original),3000);auto filteredResult=filteredWorker.waitFor(filteredSeq,std::chrono::seconds(3));tecmo98::NativeFrame expectedScaled;tecmo98::scale2x(expected,expectedScaled);
 if(!filteredResult||!filteredResult->error.empty()||filteredResult->frame.pixels!=expected.pixels||filteredResult->presentation.pixels!=expectedScaled.pixels||filteredResult->presentation.width!=expectedScaled.width)throw std::runtime_error("Asynchronous pixel filter changed pixels or replaced raw frame");
 const auto* rawStorage=filteredResult->frame.pixels.data();const auto* scaledStorage=filteredResult->presentation.pixels.data();
 filteredWorker.recycleFrames(std::move(filteredResult->frame),std::move(filteredResult->presentation));
 auto reused=filteredWorker.waitFor(filteredWorker.submit(view(original),3001),std::chrono::seconds(3));
 if(!reused||!reused->error.empty()||reused->frame.pixels.data()!=rawStorage||reused->presentation.pixels.data()!=scaledStorage||reused->frame.pixels!=expected.pixels||reused->presentation.pixels!=expectedScaled.pixels)throw std::runtime_error("Recycled buffers reallocated or changed pixels");
 filteredWorker.recycleFrames(std::move(reused->frame),std::move(reused->presentation));b=original;set(b[5],0,0);
 auto blank=filteredWorker.waitFor(filteredWorker.submit(view(b),3002),std::chrono::seconds(3));
 if(!blank||!blank->error.empty()||std::any_of(blank->frame.pixels.begin(),blank->frame.pixels.end(),[](auto p){return p!=0;})||std::any_of(blank->presentation.pixels.begin(),blank->presentation.pixels.end(),[](auto p){return p!=0;}))throw std::runtime_error("Disabled display retained recycled pixels");
 filteredWorker.recycleFrames(std::move(blank->frame),std::move(blank->presentation));
 auto restored=filteredWorker.waitFor(filteredWorker.submit(view(original),3003),std::chrono::seconds(3));
 if(!restored||!restored->error.empty()||restored->frame.pixels!=expected.pixels||restored->presentation.pixels!=expectedScaled.pixels)throw std::runtime_error("Recycled display failed to recover");
 filteredWorker.recycleFrames(std::move(restored->frame),std::move(restored->presentation));filteredWorker.setPresentationFilter(0);
 auto rawOnly=filteredWorker.waitFor(filteredWorker.submit(view(original),3004,98),std::chrono::seconds(3));
 if(!rawOnly||!rawOnly->error.empty()||rawOnly->sourceFrame!=98||rawOnly->frame.pixels!=expected.pixels||rawOnly->presentation.width||!rawOnly->presentation.pixels.empty())throw std::runtime_error("GPU mode retained stale CPU filter / lost source identity");
 filteredWorker.recycleFrames(std::move(rawOnly->frame),std::move(rawOnly->presentation));filteredWorker.setPresentationFilter(2);
 auto cpuAgain=filteredWorker.waitFor(filteredWorker.submit(view(original),3005,99),std::chrono::seconds(3));
 if(!cpuAgain||!cpuAgain->error.empty()||cpuAgain->sourceFrame!=99||cpuAgain->presentation.pixels!=expectedScaled.pixels)throw std::runtime_error("CPU filter fallback did not recover");
 std::cout<<"{\"passed\":true,\"pixel_equivalence_cases\":"<<cases<<",\"immutable_snapshot\":true,\"raw_and_filtered_buffers_reused\":true,\"recycled_display_off_is_black\":true,\"error_propagation_and_recovery\":true,\"latest_snapshot_delivery\":true}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
