#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <functional>
#include <vector>

namespace tecmo98 {
struct NativeVideoView {
    std::span<const std::uint8_t> display,draw,vram,color,vdp1,vdp2;
};
struct NativeFrame {unsigned width=0,height=0;std::vector<std::uint32_t> pixels;};
NativeFrame compose_native_frame(const NativeVideoView&);
void compose_native_frame_into(const NativeVideoView&,NativeFrame&);
struct RasterSample {
    std::uint64_t ticks,absoluteLine,frame;
    unsigned x,y,totalWidth,totalHeight,width,height;
    std::uint16_t status,hcounter,vcounter;
};
// Native monotonic presentation clock. No CPU cycle-accuracy claim: this preserves
// nominal NTSC raster rates/counter behavior, independently from monitor refresh.
class NativeRasterClock {
    std::uint64_t epoch_=0,generation_=0;
    std::uint16_t mode_=0;
    bool dots352_=true;
public:
    void configure(std::uint16_t mode,bool dots352,std::uint64_t now);
    RasterSample sample(std::uint64_t now)const;
    std::uint64_t generation()const{return generation_;}
};
struct CommandListState {
    int localX=0,localY=0;
    unsigned clipRight=511,clipBottom=255;
    int userLeft=0,userTop=0,userRight=511,userBottom=255;
    std::uint16_t lastCommand=0;
    unsigned commands=0;
};
class NativeVdp1Job {
    unsigned position_=0,return_=0xffffffff;
    bool busy_=false;
public:
    CommandListState state;
    void begin(){position_=0;return_=0xffffffff;state.commands=0;busy_=true;}
    bool busy()const{return busy_;}
    void step(std::span<const std::uint8_t> vram,unsigned commands,
              const std::function<void(unsigned,const CommandListState&)>& primitive);
};
void draw_vdp1_normal_sprite(std::span<const std::uint8_t> vram,unsigned position,const CommandListState& state,
                             std::span<std::uint8_t> framebuffer,bool eightBit,unsigned framebufferControl=0);
void draw_vdp1_primitive(std::span<const std::uint8_t> vram,unsigned position,const CommandListState& state,
                         std::span<std::uint8_t> framebuffer,bool eightBit,unsigned framebufferControl=0);
// Validated control-only lists are real completed GPU jobs. A primitive needs a
// drawing backend and stops explicitly; never return CEF for an unrendered sprite.
CommandListState execute_vdp1_control_list(std::span<const std::uint8_t> vram,CommandListState state={});
}
