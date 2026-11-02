#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cmath>
#include <charconv>

namespace tecmo98 {
struct LaunchSettings {
 int width=1000,height=740,fullscreen=0,stretch=0,smooth=0,volumeDb=0,muted=0,background=0;
 int volumePercent=100;bool percentVolume=false;
 double gain()const{return percentVolume?double(volumePercent)/100.0:std::pow(10.0,double(volumeDb)/20.0);}
 static LaunchSettings read(std::istream& in){
  LaunchSettings s;std::string magic,extra;int volume=0;
  if(!(in>>magic>>s.width>>s.height>>s.fullscreen>>s.stretch>>s.smooth>>volume>>s.muted>>s.background)||(magic!="TECMO98_NATIVE_V1"&&magic!="TECMO98_NATIVE_V2")||
     (in>>extra)||s.width<640||s.width>7680||s.height<480||s.height>4320||
     (magic=="TECMO98_NATIVE_V1"?(volume< -32||volume>0||s.smooth>1):(volume<0||volume>100||s.smooth>2))||s.smooth<0||
     (s.fullscreen&~1)||(s.stretch&~1)||(s.muted&~1)||(s.background&~1))
   throw std::runtime_error("Invalid native launcher settings");
  if(magic=="TECMO98_NATIVE_V2"){s.percentVolume=true;s.volumePercent=volume;}else s.volumeDb=volume;
  return s;
 }
};
// This transport carries host contacts only. It cannot read/write game RAM.
// Repeated or missing sequence numbers do not renew the 250 ms heartbeat.
class LauncherInput {
 std::array<std::array<std::uint32_t,6>,4> counters_{};
 std::array<std::array<std::uint64_t,6>,4> pulseUntil_{};
 std::uint64_t recovered_=0,ageMax_=0,packets_=0;
 std::array<unsigned,4> masks_{};std::uint64_t sequence_=0,lastSeen_=0;bool seen_=false,quit_=false;
public:
 bool accept(std::istream& in,std::uint64_t now){
  std::uint64_t seq=0,stamp=now;std::array<unsigned,4> masks;unsigned quit;std::string header,extra;
  std::array<std::array<std::uint32_t,6>,4> counts{};if(!(in>>header))return false;const bool events=header=="P2";
  if(events){if(!(in>>seq>>stamp))return false;}else{auto conversion=std::from_chars(header.data(),header.data()+header.size(),seq);if(conversion.ec!=std::errc{}||conversion.ptr!=header.data()+header.size())return false;}
  if(!(in>>masks[0]>>masks[1]>>masks[2]>>masks[3]>>quit)||quit>1)return false;
  if(events)for(auto& p:counts)for(auto& c:p)if(!(in>>c))return false;
  if(in>>extra)return false;for(auto m:masks)if(m>1023)return false;
  if(seen_&&seq<=sequence_)return false;
  if(events&&(stamp>now||now-stamp>250))return false;
  for(unsigned p=0;p<4;++p)for(unsigned a=0;a<6;++a){
   if(events&&counts[p][a]!=counters_[p][a]){pulseUntil_[p][a]=now+34;if(!(masks[p]&(1u<<(a+4))))++recovered_;}
   if(!events)pulseUntil_[p][a]=0;
  }
  counters_=counts;masks_=masks;sequence_=seq;lastSeen_=now;seen_=true;quit_=quit!=0;++packets_;ageMax_=std::max(ageMax_,now-stamp);return true;
 }
 std::uint64_t recoveredTaps()const{return recovered_;}std::uint64_t maximumPacketAge()const{return ageMax_;}std::uint64_t packets()const{return packets_;}
 bool quit()const{return quit_;}
 std::array<std::uint8_t,7> contacts(std::uint64_t now,bool enabled){
  std::array<std::uint8_t,7> result{};
  if(!enabled||!seen_||now<lastSeen_||now-lastSeen_>250){for(auto& p:pulseUntil_)p.fill(0);return result;}
  constexpr unsigned direction[]={0x20,0x10,0x80,0x40};
  for(unsigned p=0;p<4;++p){auto mask=masks_[p];for(unsigned a=0;a<6;++a)if(now<pulseUntil_[p][a])mask|=1u<<(a+4);const unsigned port=p<2?p:p+2;unsigned value=0;
   for(unsigned a=0;a<4;++a)if(mask&(1u<<a))value|=direction[a];
   for(unsigned a=4;a<(p<2?8u:7u);++a)if(mask&(1u<<a))value|=1u<<(a-4);
   if((value&0x30)==0x30)value&=~0x30u;if((value&0xc0)==0xc0)value&=~0xc0u;
   if(p<2){if(mask&0x100)result[2]|=0x10u<<p;if(mask&0x200)result[2]|=1u<<p;}
   else if(mask&0x100)value|=8;
   result[port]=std::uint8_t(value);
  }return result;
 }
};
}
