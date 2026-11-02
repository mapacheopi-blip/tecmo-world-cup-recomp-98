#include "native_video.hpp"
#include <algorithm>
#include <array>
#include <bit>

namespace tecmo98 {
namespace {
unsigned word(std::span<const std::uint8_t> s,unsigned a){const auto mask=unsigned(s.size())-1;a&=mask;return (unsigned(s[a])<<8)|s[(a+1)&mask];}
std::uint32_t rgb555(unsigned c){const auto expand=[](unsigned v){return (v<<3)|(v>>2);};return (expand(c&31)<<16)|(expand((c>>5)&31)<<8)|expand((c>>10)&31);}
}
void compose_native_frame_into(const NativeVideoView& v,NativeFrame& frame){
    if(v.display.size()!=0x40000||v.vram.size()!=0x80000||v.color.size()!=0x1000||v.vdp1.size()!=32||v.vdp2.size()!=512)throw std::invalid_argument("Invalid native video view");
    std::array<unsigned,256> registers{};for(unsigned i=0;i<registers.size();++i)registers[i]=(unsigned(v.vdp2[i*2])<<8)|v.vdp2[i*2+1];
    const auto r=[&](unsigned a){return registers[(a&511)>>1];};const auto mem=[&](unsigned a){return (unsigned(v.vram[a&0x7ffff])<<8)|v.vram[(a+1)&0x7ffff];};
    const auto mode=r(0);const bool high=(mode&2)!=0,doubled=(mode&0xc0)==0xc0;
    frame.width=((mode&1)!=0?352:320)*(high?2:1);frame.height=(224+((mode>>4)&3)*16)*(doubled?2:1);frame.pixels.resize(frame.width*frame.height);
    if((mode&0x8000)==0){std::fill(frame.pixels.begin(),frame.pixels.end(),0);return;}
    const auto colorFormat=(r(0xe)>>12)&3;const unsigned paletteMask=colorFormat==1?2047:1023;
    std::array<std::uint32_t,2048> colors{};
    for(unsigned i=0;i<colors.size();++i){if(colorFormat==2){const auto a=(i&1023)*4;colors[i]=(unsigned(v.color[a+3])<<16)|(unsigned(v.color[a+2])<<8)|v.color[a+1];}
        else colors[i]=rgb555(word(v.color,(i&paletteMask)*2));}
    const auto fade=[&](std::uint32_t c,unsigned layer){if((r(0x110)&(1u<<layer))==0)return c;const auto a=(r(0x112)&(1u<<layer))!=0?0x11a:0x114;unsigned out=0;for(unsigned i=0;i<3;++i){const unsigned s=r(a+i*2);const int add=int(s&255)-int(s&256);out|=unsigned(std::clamp(int((c>>(16-i*8))&255)+add,0,255))<<(16-i*8);}return out;};
    std::array<std::array<std::uint32_t,2048>,7> layerColors;
    for(unsigned layer=0;layer<7;++layer)for(unsigned i=0;i<2048;++i)layerColors[layer][i]=fade(colors[i],layer);
    const auto palette=[&](unsigned index,unsigned layer){if(colorFormat==3)throw std::runtime_error("Reserved VDP2 color RAM mode");return layerColors[layer][index&paletteMask];};
    const auto backAddress=(((r(0xac)&7)<<16)|r(0xae))*2;
    for(unsigned y=0;y<frame.height;++y){const auto c=fade(rgb555(mem(backAddress+((r(0xac)&0x8000)!=0?y*2:0))),5);std::fill_n(frame.pixels.begin()+y*frame.width,frame.width,c);}
    std::vector<std::uint8_t> priorities(frame.pixels.size());
    const auto dword=[&](unsigned a){return (std::uint32_t(mem(a))<<16)|mem(a+2);};
    const auto windowAllows=[&](unsigned control,unsigned x,unsigned y){
        if((control&0x20)!=0)throw std::runtime_error("VDP2 sprite-window masking pending");
        bool result=true,enabled=false;
        for(unsigned i=0;i<2;++i)if((control&(2u<<(i*2)))!=0){const unsigned a=0xc0+i*8;
            const unsigned left=r(a)&1023,right=r(a+4)&1023,top=r(a+2)&2047,bottom=r(a+6)&2047;
            const bool inside=x>=(high?left:left/2)&&x<=(high?right:right/2)&&y>=top&&y<=bottom;
            const bool visible=inside==((control&(1u<<(i*2)))!=0);
            if(!enabled){result=visible;enabled=true;}else result=(control&0x80)!=0?result||visible:result&&visible;}
        return result;
    };
    if((r(0x20)&0x20)!=0)throw std::runtime_error("VDP2 RBG1 screen pending");
    if((r(0x20)&0x10)!=0&&(r(0xb0)&3)==3)throw std::runtime_error("VDP2 rotation parameter window selection pending");
    // Parameter B is drawn first in coefficient-selection mode. Valid A dots
    // then replace it; the coefficient MSB makes an A dot transparent.
    for(int layer=5;layer>=0;--layer){
        const bool rotation=layer>=4;const unsigned rp=layer==5?1:0,logical=rotation?4:unsigned(layer),rotationMode=r(0xb0)&3;
        if((r(0x20)&(1u<<logical))==0)continue;
        if(rotation&&((rotationMode==0&&rp==1)||(rotationMode==1&&rp==0)))continue;
        const auto priority=rotation?r(0xfc)&7:(r(layer<2?0xf8:0xfa)>>((layer&1)*8))&7;if(priority==0)continue;
        const auto ch=rotation?r(0x2a)>>8:layer<2?r(0x28)>>(layer*8):r(0x2a)>>((layer-2)*4);
        const unsigned depth=rotation||layer==0?(ch>>4)&7:layer==1?(ch>>4)&3:(ch>>1)&1;
        const bool bitmap=(rotation||layer<2)&&(ch&2)!=0,tile16=(ch&1)!=0,opaque=(r(0x20)&(0x100u<<logical))!=0;
        const auto pn=r(rotation?0x38:0x30+layer*2),plane=(r(0x3a)>>(rotation?8+rp*4:layer*2))&3,offset=rotation?(r(0x3e)>>(rp*4))&7:(r(0x3c)>>(layer*4))&7,crOffset=rotation?(r(0xe6)&7)*256:((r(0xe4)>>(layer*4))&7)*256;
        if(plane==2&&!bitmap)throw std::runtime_error("Reserved VDP2 plane size");
        const auto scroll=layer<2?(r(0x9a)>>(layer*8))&63:0;if((scroll&1)!=0)throw std::runtime_error("VDP2 vertical cell scroll compositor pending");
        const unsigned window=rotation?r(0xd4)&255:(r(layer<2?0xd0:0xd2)>>((layer&1)*8))&255;
        const unsigned sx=rotation?0:r(layer<2?0x70+layer*0x10:0x90+(layer-2)*4),sy=rotation?0:r(layer<2?0x74+layer*0x10:0x92+(layer-2)*4);
        const unsigned zx=layer<2?((r(0x78+layer*0x10)<<16)|r(0x7a+layer*0x10))&0x7ff00:65536,zy=layer<2?((r(0x7c+layer*0x10)<<16)|r(0x7e +layer*0x10))&0x7ff00:65536;
        const unsigned tile=tile16?16:8,tiles=512/tile,entry=(pn&0x8000)!=0?2:4,pageBytes=tiles*tiles*entry,pagesX=(plane&1)!=0?2:1,pagesY=(plane&2)!=0?2:1;
        const unsigned planeW=pagesX*512,planeH=pagesY*512,planeBytes=pageBytes*pagesX*pagesY,planeXShift=9+(pagesX==2),planeYShift=9+(pagesY==2),tileShift=tile16?4:3;
        std::array<std::int64_t,24> parameters{};
        const auto mul=[](std::int64_t a,std::int64_t b){return (a*b)>>16;};
        if(rotation){const unsigned table=((((r(0xbc)&7)<<16)|r(0xbe))*2&~128u)+rp*128;
            const auto signedField=[](unsigned raw,unsigned bits,unsigned low){const unsigned mask=(1u<<bits)-1,value=(raw&mask)&~((1u<<low)-1);return std::int64_t(std::int32_t((value&(1u<<(bits-1)))!=0?value|~mask:value));};
            for(unsigned i=0;i<24;++i){const unsigned bits=i<3?29:i<7?19:i<13?20:i==17||i==18?30:i==19||i==20?24:i==21?32:26;
                parameters[i]=bits==32?dword(table+i*4)&0xffffffc0u:signedField(dword(table+i*4),bits,i==19||i==20?0:6);}
            parameters[13]=signedField(mem(table+52),14,0)*65536;parameters[14]=signedField(mem(table+54),14,0)*65536;
            parameters[15]=signedField(mem(table+56),14,0)*65536; // Pz
            parameters[16]=signedField(mem(table+60),14,0)*65536; // Cx
            // C_y and C_z are held separately below: table packing is 16-bit.
        }
        const unsigned rotationTable=((((r(0xbc)&7)<<16)|r(0xbe))*2&~128u)+rp*128,reload=(r(0xb2)>>(rp*8))&7,ctl=(r(0xb4)>>(rp*8))&255,over=(r(0x3a)>>(10+rp*4))&3;
        const auto signedCoordinate=[&](unsigned a){const unsigned value=mem(a)&0x3fff;return std::int64_t(int(value&0x1fff)-int(value&0x2000))*65536;};
        const std::array<std::int64_t,3> pivot={parameters[13],parameters[14],parameters[15]},center={parameters[16],rotation?signedCoordinate(rotationTable+62):0,rotation?signedCoordinate(rotationTable+64):0};
        std::int64_t rotationOriginX=center[0]+parameters[17],rotationOriginY=center[1]+parameters[18];
        if(rotation)for(unsigned i=0;i<3;++i){rotationOriginX+=mul(parameters[7+i],pivot[i]-center[i]);rotationOriginY+=mul(parameters[10+i],pivot[i]-center[i]);}
        const bool smallCoefficient=(ctl&2)!=0;const unsigned coefficientBase=((r(0xb6)>>(rp*8))&(smallCoefficient?7:3))*(smallCoefficient?0x20000:0x40000);
        struct Coefficient {bool visible=true;std::int64_t kx,ky,originX;};
        const auto coefficientAt=[&](std::int64_t index){Coefficient result{true,parameters[19],parameters[20],rotationOriginX};
            if((ctl&1)!=0){const unsigned ca=coefficientBase+unsigned(index)*(smallCoefficient?2:4);const auto cm=(r(0xe)&0x8000)!=0?v.color:v.vram;
                const unsigned raw=smallCoefficient?word(cm,ca):(word(cm,ca)<<16)|word(cm,ca+2);if((raw&(smallCoefficient?0x8000u:0x80000000u))!=0){result.visible=false;return result;}
                const unsigned bits=smallCoefficient?15:24,mask=(1u<<bits)-1,value=raw&mask;const std::int64_t valueSigned=std::int32_t((value&(1u<<(bits-1)))!=0?value|~mask:value)*(smallCoefficient?64:1);
                switch((ctl>>2)&3){case 0:result.kx=result.ky=valueSigned;break;case 1:result.kx=valueSigned;break;case 2:result.ky=valueSigned;break;case 3:result.originX=valueSigned;break;}}
            return result;
        };
        const bool wholePixelStep=(parameters[5]&65535)==0&&(parameters[6]&65535)==0;
        const auto uStep=mul(parameters[7],parameters[5])+mul(parameters[8],parameters[6]),wStep=mul(parameters[10],parameters[5])+mul(parameters[11],parameters[6]);
        // Rotation samples use low-resolution dots. With absent windows or
        // boundaries aligned to these groups, high-resolution/interlaced
        // 2x2 dots share source and priority; evaluate the source once.
        bool alignedWindow=(window&0x20)==0;
        for(unsigned i=0;i<2;++i)if((window&(2u<<(i*2)))!=0){const unsigned a=0xc0+i*8;if(high&&(((r(a)&1)!=0&&r(a)<frame.width)||((r(a+4)&1)==0&&r(a+4)<frame.width-1)))alignedWindow=false;if(doubled&&(((r(a+2)&1)!=0&&r(a+2)<frame.height)||((r(a+6)&1)==0&&r(a+6)<frame.height-1)))alignedWindow=false;}
        const unsigned blockX=rotation&&high&&alignedWindow?2:1,blockY=rotation&&doubled&&alignedWindow?2:1;
        for(unsigned y=0;y<frame.height;y+=blockY){
            unsigned lineX=0,lineY=0,lineZoom=zx;if((scroll&14)!=0){const unsigned count=unsigned((scroll&2)!=0)+unsigned((scroll&4)!=0)+unsigned((scroll&8)!=0);unsigned a=(((r(0xa0+layer*4)&7)<<16)|r(0xa2+layer*4))*2+(y>>(scroll>>4))*count*4;auto value=[&](){const auto d=(mem(a)<<16)|mem(a+2);a+=4;return d;};if((scroll&2)!=0)lineX=value()>>16;if((scroll&4)!=0)lineY=value()>>16;if((scroll&8)!=0)lineZoom=value()&0x7ff00;}
            const std::array<std::int64_t,3> rowPoint={parameters[0]+((reload&1)!=0?0:parameters[3]*(y>>(doubled?1:0)))-pivot[0],parameters[1]+((reload&2)!=0?0:parameters[4]*(y>>(doubled?1:0)))-pivot[1],parameters[2]-pivot[2]};
            const auto rowCoefficientIndex=parameters[21]+((reload&4)!=0?0:parameters[22]*(y>>(doubled?1:0)));
            const auto rowCoefficient=rotation?coefficientAt(rowCoefficientIndex>>16):Coefficient{true,0,0,0};
            std::int64_t uRow=0,wRow=0;if(rotation)for(unsigned i=0;i<3;++i){uRow+=mul(parameters[7+i],rowPoint[i]);wRow+=mul(parameters[10+i],rowPoint[i]);}
            for(unsigned x=0;x<frame.width;x+=blockX){const auto at=y*frame.width+x;if(priorities[at]>priority)continue;
            if(!windowAllows(window,x,y))continue;
            bool overPattern=false;
            unsigned px=sx+lineX+unsigned((std::uint64_t(x)*lineZoom)>>16),py=sy+lineY+unsigned((std::uint64_t(y)*zy)>>16),address=0,pal=0,index=0;
            if(rotation){const auto& p=parameters;
                const auto coefficient=p[23]==0?rowCoefficient:coefficientAt((rowCoefficientIndex+p[23]*(x>>(high?1:0)))>>16);if(!coefficient.visible)continue;
                const auto coordinateX=x>>(high?1:0);std::int64_t u,w;
                if(wholePixelStep){u=uRow+uStep*coordinateX;w=wRow+wStep*coordinateX;}
                else {const std::array<std::int64_t,3> point={rowPoint[0]+p[5]*coordinateX,rowPoint[1]+p[6]*coordinateX,rowPoint[2]};u=w=0;for(unsigned i=0;i<3;++i){u+=mul(p[7+i],point[i]);w+=mul(p[10+i],point[i]);}}
                const auto tx=(mul(coefficient.kx,u)+coefficient.originX)>>16,ty=(mul(coefficient.ky,w)+rotationOriginY)>>16;
                if(over==1){const unsigned bw=bitmap?((ch&8)!=0?1024:512):planeW*4,bh=bitmap?((ch&4)!=0?512:256):planeH*4;
                    overPattern=tx<0||ty<0||tx>=bw||ty>=bh;}
                const unsigned boundX=over==3?512:planeW*4,boundY=over==3?512:planeH*4;if(over>=2&&(tx<0||ty<0||tx>=boundX||ty>=boundY))continue;
                px=unsigned(tx)&(planeW*4-1);py=unsigned(ty)&(planeH*4-1);
            }
            if(bitmap&&!overPattern){const unsigned size=(ch>>2)&(rotation?1:3),w=(size&2)!=0?1024:512,h=(size&1)!=0?512:256;px&=w-1;py&=h-1;address=offset*0x20000;index=py*w+px;pal=(rotation?r(0x2e)&7:(r(0x2c)>>(layer*8))&7)*256;}
            else {px&=planeW*(rotation?4:2)-1;py&=planeH*(rotation?4:2)-1;const unsigned map=(px>>planeXShift)+(py>>planeYShift)*(rotation?4:2),mp=(r((rotation?0x50+rp*16:0x40+layer*4)+(map/2)*2)>>((map&1)*8))&63;
                const unsigned mask=0x1ff>>((entry==4?1:0)|(tile16?0:2)),shift=plane==0?0:plane==1?1:2;
                const unsigned base=(((offset*64+mp)&mask)>>shift)*planeBytes;
                px&=planeW-1;py&=planeH-1;const unsigned page=(px>>9)+(py>>9)*pagesX,ea=base+page*pageBytes+(((py&511)>>tileShift)*tiles+((px&511)>>tileShift))*entry;
                unsigned character,flip=0;
                if(entry==2||overPattern){const auto data=overPattern?r(0xb8+rp*2):mem(ea),scn=pn&31;if((pn&0x4000)!=0)character=tile16?((data&0xfff)<<2)+(scn&3)+((scn&16)<<10):(data&0xfff)+((scn&28)<<10);
                    else {flip=(data>>10)&3;character=tile16?((data&0x3ff)<<2)+(scn&3)+((scn&28)<<10):(data&0x3ff)+(scn<<10);}
                    pal=depth==0?(((data>>12)&15)+(((pn>>5)&7)<<4))*16:(data&0x7000)>>4;
                }else {const auto data=(mem(ea)<<16)|mem(ea+2);character=data&0x7fff;flip=(data>>30)&3;pal=((data>>16)&127)*16;}
                unsigned tx=px&(tile-1),ty=py&(tile-1);if((flip&1)!=0)tx=tile-1-tx;if((flip&2)!=0)ty=tile-1-ty;
                const unsigned bytesPerTile=depth==0?32:depth==1?64:depth==4?256:128;
                address=(character&0x3fff)*32+(tile16?(tx/8+(ty/8)*2)*bytesPerTile:0);index=(ty%8)*8+(tx%8);
            }
            unsigned raw=0,c=0;
            if(depth==0){const auto b=v.vram[(address+index/2)&0x7ffff];raw=(index&1)!=0?b&15:b>>4;c=palette(pal+raw+crOffset,logical);}
            else if(depth==1){raw=v.vram[(address+index)&0x7ffff];c=palette((pal&~255u)+raw+crOffset,logical);}
            else if(depth==2){raw=mem(address+index*2)&2047;c=palette(raw+crOffset,logical);}
            else if(depth==3){raw=mem(address+index*2);c=rgb555(raw);if(!opaque&&(raw&0x8000)==0)continue;}
            else if(depth==4){const auto a=(address+index*4)&0x7ffff;raw=mem(a);c=(unsigned(v.vram[(a+3)&0x7ffff])<<16)|(unsigned(v.vram[(a+2)&0x7ffff])<<8)|v.vram[(a+1)&0x7ffff];if(!opaque&&(raw&0x8000)==0)continue;}
            else throw std::runtime_error("Reserved VDP2 color depth");
            if(!opaque&&depth<3&&raw==0)continue;const auto result=depth<3?c:fade(c,logical);for(unsigned by=0;by<blockY;++by)for(unsigned bx=0;bx<blockX;++bx){const auto dest=at+by*frame.width+bx;frame.pixels[dest]=result;priorities[dest]=priority;}
        }}
    }
    constexpr std::array<unsigned,16> masks={0x7ff,0x7ff,0x7ff,0x7ff,0x3ff,0x7ff,0x3ff,0x1ff,0x7f,0x3f,0x3f,0x3f,0xff,0xff,0xff,0xff};
    constexpr std::array<unsigned,16> shifts={14,13,14,13,13,12,12,12,7,7,6,0,7,7,6,0},pmasks={3,7,1,3,3,7,7,7,1,1,3,0,1,1,3,0};
    const auto type=r(0xe0)&15,mask=masks[type];const bool eight=(word(v.vdp1,0)&1)!=0;const unsigned stride=eight?1024:512;
    for(unsigned y=0;y<frame.height;++y)for(unsigned x=0;x<frame.width;++x){const unsigned fy=doubled?y/2:y,fx=high&&!eight?x/2:x;if(fy>=256||fx>=stride)continue;const auto a=(fy*stride+fx)*(eight?1:2);unsigned raw=eight?v.display[a]:word(v.display,a);if(raw==0)continue;
        const bool direct=(raw&0x8000)!=0&&(r(0xe0)&0x20)!=0;const auto p=direct?0:(raw>>shifts[type])&pmasks[type],priority=(r(0xf0+(p/2)*2)>>((p&1)*8))&7;
        const auto at=y*frame.width+x;if(priority==0||priority<priorities[at]||!windowAllows((r(0xd4)>>8)&255,x,y))continue;
        raw=direct?raw:raw&mask;if(!direct&&(raw==0||raw==mask-1))continue;
        frame.pixels[at]=direct?fade(rgb555(raw),6):palette(raw+(((r(0xe6)>>4)&7)*256),6);
    }
}
NativeFrame compose_native_frame(const NativeVideoView& v){
    NativeFrame frame;compose_native_frame_into(v,frame);return frame;
}
}
