#include "native_video.hpp"
#include <array>
#include <algorithm>
#include <iostream>
struct P{int x,y;};
static int cross(P a,P b,P c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
static bool inside(const std::array<P,4>& q,P p){bool plus=false,minus=false;for(unsigned i=0;i<4;++i){const auto c=cross(q[i],q[(i+1)%4],p);plus|=c>0;minus|=c<0;}return !(plus&&minus);}
int main(){try{
 std::vector<std::uint8_t> vram(0x80000),fb(0x40000);const auto word=[&](unsigned a,unsigned v){vram[a]=v>>8;vram[a+1]=v;};
 const auto pixel=[&](unsigned x,unsigned y){const auto a=(y*512+x)*2;return (unsigned(fb[a])<<8)|fb[a+1];};
 unsigned cases=0;
 for(int turn=0;turn<2;++turn)for(int ax=1;ax<=5;++ax)for(int ay=1;ay<=4;++ay){
  std::array<P,4> q={P{8,8},P{8+ax,8-ay},P{8+ax+4,8-ay+6},P{12,14}};if(turn)std::reverse(q.begin(),q.end());
  for(unsigned kind:{2u,4u})for(unsigned mode:{0xc0u,0x1c0u})for(unsigned field:{0u,8u,12u}){
   std::fill(fb.begin(),fb.end(),0);word(0,kind);word(4,mode);word(6,kind==4?0x801f:0x8010);word(8,0x80);word(10,0x108);std::fill_n(vram.begin()+0x400,32,0xff);
   for(unsigned i=0;i<4;++i){word(12+i*4,q[i].x&0x7ff);word(14+i*4,q[i].y&0x7ff);}
   tecmo98::draw_vdp1_primitive(vram,0,{},fb,false,field);
   for(unsigned y=0;y<24;++y)for(unsigned x=0;x<24;++x){if(field&&((y&1)!=((field>>2)&1)))continue;
    bool expected=inside(q,{int(x),int(y)})&&(!(mode&0x100)||((x^y)&1)!=0);const auto actual=pixel(x,field?y/2:y);
    if(actual!=(expected?0x801fu:0u))throw std::runtime_error("Polygon coverage hole/overspill at "+std::to_string(x)+","+std::to_string(y)+" case "+std::to_string(cases));
   }++cases;
  }
 }
 std::cout<<"{\"passed\":true,\"convex_polygons\":"<<cases<<",\"independent_point_inside_oracle\":true,\"mesh_and_interlace_preserved\":true}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
