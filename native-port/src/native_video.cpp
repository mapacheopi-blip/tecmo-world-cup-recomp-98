#include "native_video.hpp"
#include <string>

namespace tecmo98 {
void NativeRasterClock::configure(std::uint16_t mode,bool dots352,std::uint64_t now) {
    if((mode&4)!=0)throw std::runtime_error("Exclusive-monitor video timing pending: TVMD="+std::to_string(mode));
    if(mode_!=mode||dots352_!=dots352||generation_==0){mode_=mode;dots352_=dots352;epoch_=now;++generation_;}
}
RasterSample NativeRasterClock::sample(std::uint64_t now)const {
    if(now<epoch_)throw std::runtime_error("Native video clock moved backwards");
    const auto elapsed=now-epoch_;const unsigned horizontal=mode_&3;
    const unsigned interlace=(mode_&0xc0)==0xc0?2:1;
    const unsigned divider=((horizontal&2)!=0?4:8)/interlace;
    const std::uint64_t master=dots352_?57272724:53693178;
    const auto frequency=master/divider;
    const auto ticks=(elapsed/1000000000)*frequency+((elapsed%1000000000)*frequency)/1000000000;
    const unsigned width=(horizontal&1)!=0?352:320,totalWidth=(horizontal&1)!=0?455:427;
    const unsigned scale=(horizontal&2)!=0?2:1,height=((mode_&0x10)!=0?240:224)*interlace;
    RasterSample state{ticks,ticks/(totalWidth*scale),0,0,0,totalWidth*scale,263*interlace,width*scale,height,0,0,0};
    state.frame=state.absoluteLine/state.totalHeight;state.y=unsigned(state.absoluteLine%state.totalHeight);state.x=unsigned(ticks%state.totalWidth);
    const auto odd=unsigned((state.frame&1)^1); // Resolution configuration initializes ODD=1 in the reference.
    state.status=std::uint16_t(((state.y>=height||(mode_&0x8000)==0)?8:0)|(state.x>=state.width?4:0)|(odd<<1));
    state.hcounter=std::uint16_t((horizontal&2)!=0?state.x&0x3ff:(state.x&0x1ff)<<1);
    const auto wrap=(mode_&0x10)!=0?0xf5:0xec;
    state.vcounter=std::uint16_t(interlace==2?(state.y&~1u)|(state.frame&1):state.y+(state.y>unsigned(wrap)?0xf9:0));
    return state;
}
void NativeVdp1Job::step(std::span<const std::uint8_t> vram,unsigned budget,const std::function<void(unsigned,const CommandListState&)>& primitive) {
    if(vram.size()!=0x80000)throw std::invalid_argument("VDP1 command VRAM size mismatch");
    const auto word=[&](unsigned at){if(at+2>vram.size())throw std::out_of_range("VDP1 command outside VRAM");return std::uint16_t((vram[at]<<8)|vram[at+1]);};
    for(unsigned count=0;count<budget&&busy_;++count){
        const auto position=position_;
        if(position+32>vram.size())throw std::out_of_range("VDP1 truncated command");
        const auto control=word(position);state.lastCommand=std::uint16_t(position/8);++state.commands;
        if((control&0x8000)!=0){busy_=false;return;}
        if((control&0x4000)==0){
            switch(control&15){
                case 8:state.userLeft=word(position+12);state.userTop=word(position+14);state.userRight=word(position+20);state.userBottom=word(position+22);break;
                case 9:state.clipRight=word(position+20)&0x3ff;state.clipBottom=word(position+22)&0x1ff;break;
                case 10:state.localX=std::int16_t(word(position+12));state.localY=std::int16_t(word(position+14));break;
                default:primitive(position,state);break;
            }
        }
        const auto jump=(control>>12)&3;
        if(jump==0)position_=position+32;
        else if(jump==1)position_=unsigned(word(position+2))*8;
        else if(jump==2){if(return_!=0xffffffff)throw std::runtime_error("Nested VDP1 call needs validation");return_=position+32;position_=unsigned(word(position+2))*8;}
        else {if(return_==0xffffffff)throw std::runtime_error("VDP1 return without call");position_=return_;return_=0xffffffff;}
        position_&=0x7ffff;
    }
}
CommandListState execute_vdp1_control_list(std::span<const std::uint8_t> vram,CommandListState state) {
    NativeVdp1Job job;job.state=state;job.begin();
    job.step(vram,16384,[](unsigned position,const CommandListState&){throw std::runtime_error("VDP1 primitive backend pending at "+std::to_string(position));});
    if(job.busy())throw std::runtime_error("VDP1 command loop did not reach draw end");return job.state;
}
void draw_vdp1_normal_sprite(std::span<const std::uint8_t> vram,unsigned position,const CommandListState& state,
                             std::span<std::uint8_t> framebuffer,bool eightBit,unsigned framebufferControl) {
    if(vram.size()!=0x80000||framebuffer.size()!=0x40000||position>0x7ffe0)throw std::invalid_argument("Invalid native sprite buffers/command range");
    const auto word=[&](unsigned at){at&=0x7ffff;return unsigned((vram[at]<<8)|vram[(at+1)&0x7ffff]);};
    const auto control=word(position),size=word(position+10),width=((size>>8)&63)*8,height=size&255;
    if((control&15)!=0)throw std::runtime_error("VDP1 primitive type pending: "+std::to_string(control&15)+" at="+std::to_string(position)+" PMOD="+std::to_string(word(position+4)));
    if(width==0||height==0)return; // Valid no-pixel job; does not imply end of the command list.
    const auto mode=word(position+4),color=word(position+6),source=word(position+8)*8,format=(mode>>3)&7;
    if((mode&7)!=0||format>5)throw std::runtime_error("VDP1 sprite color calculation pending: PMOD="+std::to_string(mode));
    const auto coordinate=[](unsigned value){return int(value&0x3ff)-int(value&0x400);};
    const int originX=coordinate(word(position+12))+state.localX,originY=coordinate(word(position+14))+state.localY;
    const unsigned stride=eightBit?1024:512;
    for(unsigned y=0;y<height;++y){const int rowY=originY+int(y);const bool field=(framebufferControl&8)!=0;
        if(rowY<0||rowY>=(field?512:256)||rowY>int(state.clipBottom)||(field&&unsigned(rowY&1)!=((framebufferControl>>2)&1)))continue;
        unsigned endcodes=0;for(unsigned x=0;x<width;++x){
        const auto tx=(control&0x10)!=0?width-1-x:x,ty=(control&0x20)!=0?height-1-y:y,index=ty*width+tx;
        unsigned raw=0,pixel=0,endcode=0;
        if(format<=1){const auto packed=vram[(source+index/2)&0x7ffff];raw=(index&1)!=0?packed&15:packed>>4;endcode=15;pixel=format==0?(color&0xfff0)|raw:word(color*8+raw*2);}
        else if(format<5){const unsigned mask=format==2?63:format==3?127:255;raw=vram[(source+index)&0x7ffff];endcode=255;pixel=(color&~mask)|(raw&mask);}
        else {raw=word(source+index*2);endcode=0x7fff;pixel=raw;}
        if((mode&0x80)==0&&raw==endcode){if(++endcodes==2)break;continue;}
        if((mode&0x40)==0&&(format==5?(raw&0x8000)==0:raw==0))continue;
        const int dx=originX+int(x),dy=originY+int(y);
        const bool interlaced=(framebufferControl&8)!=0;
        if(dx<0||dy<0||dx>=int(stride)||dy>=(interlaced?512:256)||dx>int(state.clipRight)||dy>int(state.clipBottom))continue;
        if(interlaced&&unsigned(dy&1)!=((framebufferControl>>2)&1))continue;
        const bool userInside=dx>=state.userLeft&&dy>=state.userTop&&dx<=state.userRight&&dy<=state.userBottom;
        if((mode&0x400)!=0&&(userInside==((mode&0x200)!=0)))continue;
        if((mode&0x100)!=0&&((dx^dy)&1)==0)continue;
        if((mode&0x8000)!=0)pixel|=0x8000;
        const auto at=(unsigned(interlaced?dy/2:dy)*stride+unsigned(dx))*(eightBit?1:2);
        if(eightBit)framebuffer[at]=std::uint8_t(pixel);else {framebuffer[at]=std::uint8_t(pixel>>8);framebuffer[at+1]=std::uint8_t(pixel);}
    }}
}
}
