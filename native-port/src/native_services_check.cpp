#include "native_session.hpp"
#include "native_dma.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>

namespace {
std::uint32_t u32(std::istream& in){std::uint8_t b[4];if(!in.read(reinterpret_cast<char*>(b),4))throw std::runtime_error("Truncated service fixture");return b[0]|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
void check(bool passed,const char* message){if(!passed)throw std::runtime_error(message);}
void presentationContracts(){
    std::vector<std::uint8_t> display(0x40000),vram(0x80000),color(4096);std::array<std::uint8_t,32> vdp1{};std::array<std::uint8_t,512> vdp2{};
    const auto set=[](auto& bytes,unsigned at,unsigned value){bytes[at]=std::uint8_t(value>>8);bytes[at+1]=std::uint8_t(value);};
    set(vdp2,0,0x8000);set(vdp2,0xae,0x4000);set(vram,0x8000,0x7c00);
    tecmo98::NativeVideoView view{display,display,vram,color,vdp1,vdp2};auto frame=tecmo98::compose_native_frame(view);
    check(frame.width==320&&frame.height==224&&frame.pixels[0]==0x0000ff,"VDP2 back-screen color/geometry differ");
    set(vdp2,0x20,1);set(vdp2,0x30,0x8000);set(vdp2,0x78,1);set(vdp2,0x7c,1);set(vdp2,0xf8,1);set(vram,0,1);vram[32]=0x20;set(color,4,31);
    frame=tecmo98::compose_native_frame(view);check(frame.pixels[0]==0xff0000&&frame.pixels[1]==0x0000ff,"VDP2 tile pattern/palette/transparency differ");
    set(vdp1,0,1);set(vdp2,0xe0,14);set(vdp2,0xf0,2);display[0]=3;set(color,6,0x03e0);
    frame=tecmo98::compose_native_frame(view);check(frame.pixels[0]==0x00ff00&&frame.pixels[1]==0x0000ff,"VDP1 framebuffer/VDP2 sprite priority differ");
    display[0]=0;set(vdp2,0x20,0x10);set(vdp2,0xfc,1);set(vdp2,0x38,0x8000);set(vdp2,0xbc,0);set(vdp2,0xbe,0x2000);
    const auto set32=[&](unsigned a,unsigned value){set(vram,a,value>>16);set(vram,a+2,value);};
    for(unsigned parameter=0;parameter<2;++parameter)for(unsigned index:{4u,5u,7u,11u,19u,20u})set32(0x4000+parameter*128+index*4,65536);
    frame=tecmo98::compose_native_frame(view);check(frame.pixels[0]==0xff0000&&frame.pixels[1]==0x0000ff,"VDP2 rotation identity/parameter A differ");
    for(unsigned row=0;row<8;++row){vram[32+row*4]=0x23;vram[33+row*4]=0x40;}set(color,8,0x7c00);
    set32(0x4000,65536);set32(0x4000+3*4,65536);
    frame=tecmo98::compose_native_frame(view);check(frame.pixels[0]==0x00ff00&&frame.pixels[frame.width]==0x0000ff,"VDP2 first-line start/increment were discarded");
    set(vdp2,0xb2,1);frame=tecmo98::compose_native_frame(view);check(frame.pixels[frame.width]==0x00ff00,"VDP2 per-line Xst reload did not reset its vertical increment");
    set(vdp2,0xb2,0);set32(0x4000,0);set32(0x4000+3*4,0);
    set(vdp2,0xb4,3);set32(0x4000+21*4,256u<<16);set(vram,512,0x800);vram[32]=0x23;vram[33]=0x40;set(color,8,0x7c00);
    frame=tecmo98::compose_native_frame(view);check(frame.pixels[0]==0xff0000&&frame.pixels[1]==0x0000ff,"VDP2 coefficient table address/scale differ");
    set(vdp2,0xb0,2);set(vdp2,0x60,1);set(vram,0x2000,2);vram[64]=0x30;set(vram,512,0x8000);
    frame=tecmo98::compose_native_frame(view);check(frame.pixels[0]==0x00ff00,"VDP2 coefficient MSB did not select parameter B");
    set(vdp2,0xb0,0);set(vdp2,0xb4,0);set(vdp2,0x3a,0x400);set(vdp2,0xb8,2);set32(0x4000,2048u<<16);
    frame=tecmo98::compose_native_frame(view);check(frame.pixels[0]==0x00ff00,"VDP2 screen-over pattern name was not repeated outside the rotation map");
    set32(0x4000,0);frame=tecmo98::compose_native_frame(view);check(frame.pixels[0]==0xff0000,"VDP2 screen-over pattern replaced a valid map dot");
    tecmo98::NativeScsp sound;std::vector<std::uint8_t> samples(0x100000);samples[0]=0;samples[1]=127;samples[2]=128;samples[3]=64;sound.attachRam(samples);
    sound.write(0x400,15,0);sound.write(6,4,0);sound.write(12,0x100,0);sound.write(22,0xe000,0);sound.write(0,0x1810,0);sound.poll(100000);
    const auto audio=sound.takeAudio();const std::vector<std::int16_t> expected{0,0,32512,32512,-32768,-32768,0,0};
    check(audio==expected&&sound.nonzeroSamples()==2,"SCSP signed PCM/direct output/nominal clock/end-of-sample differ");
}
void schedulerContracts(char** argv){
    auto whole=std::make_unique<tecmo98::NativeSession>(argv[2],argv[3],argv[4],argv[5],[]{return std::uint64_t(0);});
    auto sliced=std::make_unique<tecmo98::NativeSession>(argv[2],argv[3],argv[4],argv[5],[]{return std::uint64_t(0);});
    whole->runCooperative(50000);for(unsigned i=0;i<500;++i)sliced->runCooperative(100);
    for(auto pair:{std::pair{&whole->context,&sliced->context},std::pair{&whole->slaveContext,&sliced->slaveContext}})
        check(pair.first->pc==pair.second->pc&&pair.first->abi==pair.second->abi&&pair.first->executed==pair.second->executed,"Presentation budget changed the cooperative CPU schedule/state");
    check(whole->soundContext.pc==sliced->soundContext.pc&&whole->soundContext.abi==sliced->soundContext.abi,"Presentation budget changed sound processor state");
    for(unsigned a=0x06000000;a<0x06100000;a+=4)check(whole->read(a,4)==sliced->read(a,4),"Sliced native scheduling changed shared RAM");
}
void renderPublicationContract(char** argv){
    auto session=std::make_unique<tecmo98::NativeSession>(argv[2],argv[3],argv[4],argv[5],[]{return std::uint64_t(0);});
    session->write(0x20100063,1,1);session->write(0x2010001f,1,2);
    session->context.pc=0x0605ee10;session->context.reg(0,0x260f1e68);session->context.reg(1,0x21000000);
    auto& worker=session->slaveContext;worker.pc=0x06062bca;worker.special(0x104,0x060ffc00);worker.special(0x100,0xf0);worker.reg(1,0xfffffe11);worker.reg(4,0x260f1eb4);
    session->write(0x060ffc44,4,0x260f1eb4);session->write(0x060ffc48,4,0x260f1e68);
    session->runCooperative(1);check(session->context.pc==0x0605ee12&&session->renderPublicationPending(),"Original MINIT publication did not retain the native producer barrier");
    session->runCooperative(8);check(worker.pc==0x06062b9a&&session->context.pc==0x0605ee12&&session->renderPublicationPending(),"Producer resumed before the worker captured its reset cursor");
    check(session->read(0x060ffc48,4)==0x260f1e68&&session->renderPublicationPending(),"A host inspection fabricated a worker acknowledgement");
    session->runCooperative(1);check(worker.reg(1)==0x260f1e68&&session->renderPublicationPending(),"One producer read released the barrier before the worker completed its wrap");
    session->write(0x060ffc44,4,0x260f1e68);check(session->renderPublicationPending(),"A host write fabricated a worker reset acknowledgement");
    worker.pc=0x06062bc4;worker.reg(0,0x260f1e68);session->runCooperative(1);
    check(session->read(0x060ffc44,4)==0x260f1e68&&!session->renderPublicationPending(),"Compiled worker cursor reset did not release the native publication barrier");
}
void macArchitectureContracts(char** argv){
    auto bus=std::make_unique<tecmo98::NativeSession>(argv[2],argv[3],argv[4],argv[5],[]{return std::uint64_t(0);});
    struct Case{unsigned a,b,high,low,status,resultHigh,resultLow;};
    constexpr Case cases[]={
        {0x10001,0x10001,0,0,0,1,0x20001},
        {0x10001,0x10001,0,0,2,1,0x20001},
        {0xfffeffff,0x10001,0,0,0,0xfffffffe,0xfffdffff},
        {1,1,0x1234ffff,0xffffffff,2,0x12340000,0},
        {1,1,0x12347fff,0xffffffff,2,0x12347fff,0xffffffff},
        {0xffffffff,1,0x12348000,0,2,0x12348000,0},
        {1,1,0xffffffff,0xffffffff,0,0,0}};
    for(const auto& test:cases){tecmo98::aot::Context c;c.pc=0x0605c050;c.stop=0xffffffff;c.reg(6,0x06080000);c.reg(7,0x06080004);c.special(0x100,test.status);c.special(0x10c,test.high);c.special(0x110,test.low);
        bus->write(0x06080000,4,test.a);bus->write(0x06080004,4,test.b);tecmo98::aot::run(c,*bus,1);
        check(c.special(0x10c)==test.resultHigh&&c.special(0x110)==test.resultLow&&c.reg(6)==0x06080004&&c.reg(7)==0x06080008,"Hitachi MAC.L cross products/signed 48-bit saturation/wrap differ");}
}
unsigned originalCopies(const char* path) {
    std::ifstream in(path,std::ios::binary);char magic[8];check(bool(in.read(magic,8))&&std::string(magic,8)=="T98TAIL5","Wrong reset reference");
    const auto count=u32(in);unsigned checked=0;
    for(unsigned test=0;test<count;++test){
        const auto kind=u32(in);u32(in);const auto descriptor=u32(in);u32(in);
        auto ram=std::make_unique<tecmo98::WorkRam>(),expected=std::make_unique<tecmo98::WorkRam>();
        check(bool(in.read(reinterpret_cast<char*>(ram->bytes.data()),0x1800))&&bool(in.read(reinterpret_cast<char*>(expected->bytes.data()),0x1800)),"Truncated reset reference");
        in.ignore(1448);const auto requests=u32(in);if(requests>1000)throw std::runtime_error("Invalid reset audit size");in.ignore(requests*12);
        if(kind!=3)continue;const auto source=ram->read32(descriptor+8);
        if(source<tecmo98::WorkRam::base)continue;
        std::array<std::uint32_t,4> channel={source,ram->read32(descriptor+4),ram->read32(descriptor),0x5a01};
        const auto before=channel;check(tecmo98::transfer_cpu_dma_high_ram(*ram,channel,1),"Native DMA did not copy");
        check(ram->bytes==expected->bytes,"DMA data disagrees with independent original BIOS copy");
        check(channel[0]==before[0]&&channel[1]==before[1]&&channel[2]==0&&channel[3]==0x5a03,"DMA completion registers differ");++checked;
    }
    check(checked==8,"Missing original overlapping-copy cases");return checked;
}
unsigned addressing() {
    unsigned checked=0;
    for(unsigned sizeCode=0;sizeCode<3;++sizeCode)for(unsigned srcMode=0;srcMode<3;++srcMode)for(unsigned dstMode=0;dstMode<3;++dstMode){
        auto ram=std::make_unique<tecmo98::WorkRam>();for(unsigned i=0;i<ram->bytes.size();++i)ram->bytes[i]=std::uint8_t(i*17+i/19);
        auto expected=std::make_unique<tecmo98::WorkRam>(*ram);const auto width=1u<<sizeCode,count=16u;
        const std::uint32_t source=0x06001000+(srcMode==2?0x100:0),destination=0x06002000+(dstMode==2?0x100:0);
        // Independent closed-form final destination values, using untouched input bytes.
        for(unsigned position=0;position<(dstMode==0?1u:count);++position){
            const auto iteration=dstMode==0?count-1:position;
            const auto src=srcMode==0?source:srcMode==1?source+iteration*width:source-(iteration+1)*width;
            const auto dst=dstMode==0?destination:dstMode==1?destination+position*width:destination-(position+1)*width;
            for(unsigned byte=0;byte<width;++byte)expected->write8(dst+byte,ram->read8(src+byte));
        }
        std::array<std::uint32_t,4> channel={source+width-1,destination+width-1,count,0x201|(sizeCode<<10)|(srcMode<<12)|(dstMode<<14)};
        check(tecmo98::transfer_cpu_dma_high_ram(*ram,channel,1)&&ram->bytes==expected->bytes,"DMA stride/alignment result mismatch");++checked;
    }
    return checked;
}
void failureContracts() {
    auto ram=std::make_unique<tecmo98::WorkRam>();ram->bytes.fill(0xa5);
    for(const auto control:{0x5a00u,0x5a03u}){
        std::array<std::uint32_t,4> channel={0x06001000,0x06002000,4,control};const auto before=channel;
        check(!tecmo98::transfer_cpu_dma_high_ram(*ram,channel,1)&&channel==before,"Disabled/completed DMA changed state");
    }
    for(unsigned kind=0;kind<5;++kind){
        std::array<std::uint32_t,4> channel={0x06001000,0x06002000,4,0x5a01};
        if(kind==0)channel[1]=0x060ffffc;
        if(kind==1)channel[2]=0;
        if(kind==2)channel[3]|=4;
        if(kind==3){channel[3]|=0xc00;channel[2]=5;}
        if(kind==4)channel[3]&=~0x300;
        const auto before=channel;const auto saved=std::make_unique<tecmo98::WorkRam>(*ram);bool refused=false;
        try{tecmo98::transfer_cpu_dma_high_ram(*ram,channel,1);}catch(const std::exception&){refused=true;}
        check(refused&&channel==before&&ram->bytes==saved->bytes,"Unsupported DMA changed data before rejecting");
    }
}
void videoContracts() {
    tecmo98::NativeRasterClock clock;clock.configure(0x8001,true,0);
    check(clock.sample(0).status==2&&clock.sample(0).width==352,"Initial raster/display state differs");
    check(clock.sample(50000).status==6,"Horizontal blank missing before end of first scanline");
    const auto vertical=clock.sample(15000000);
    check((vertical.status&8)!=0&&vertical.y>224&&vertical.frame==0,"Vertical blank missing before frame wrap");
    check((clock.sample(17000000).status&2)==0&&clock.sample(17000000).frame==1,"ODD did not change at frame boundary");
    check(clock.sample(16000000).vcounter>=0x1e6,"Vertical counter discontinuity missing");
    clock.configure(0x81c3,true,20000000);const auto doubled=clock.sample(20000000);
    check(doubled.width==704&&doubled.height==448&&doubled.totalHeight==526&&doubled.status==2,"Double-density video geometry differs");
    check(clock.sample(37000000).frame==1&&(clock.sample(37000000).vcounter&1)==1,"Interlaced field counter missing");
    clock.configure(1,true,40000000);check((clock.sample(40000000).status&8)!=0,"Disabled display did not report blank");
    bool backwards=false;try{clock.sample(39999999);}catch(const std::exception&){backwards=true;}check(backwards,"Backwards raster time accepted");
    std::vector<std::uint8_t> commands(0x80000);
    const auto word=[&](unsigned at,unsigned value){commands[at]=std::uint8_t(value>>8);commands[at+1]=std::uint8_t(value);};
    // CALL a local-coordinate command, RETURN, set system clipping, END.
    word(0,0x200a);word(2,12);word(12,4);word(14,5);
    word(32,9);word(52,351);word(54,223);word(64,0x8000);
    word(96,0x300a);word(108,0xfffc);word(110,9);
    const auto drawn=tecmo98::execute_vdp1_control_list(commands);
    check(drawn.commands==4&&drawn.localX==-4&&drawn.localY==9&&drawn.clipRight==351&&drawn.clipBottom==223&&drawn.lastCommand==8,"VDP1 CALL/RETURN/control state differs");
    word(0,0x100a);word(2,0);bool loop=false;try{tecmo98::execute_vdp1_control_list(commands);}catch(const std::exception&){loop=true;}check(loop,"VDP1 infinite link loop accepted");
    word(0,0);bool primitive=false;try{tecmo98::execute_vdp1_control_list(commands);}catch(const std::exception&){primitive=true;}check(primitive,"Unrendered primitive reported successful completion");
    commands.assign(0x80000,0);tecmo98::NativeVdp1Job pending;pending.begin();unsigned consumed=0;
    pending.step(commands,2,[&](unsigned,const tecmo98::CommandListState&){++consumed;});
    check(pending.busy()&&consumed==2,"Unterminated GPU job fabricated a completion");
    word(64,0x8000);pending.step(commands,1,[](unsigned,const tecmo98::CommandListState&){throw std::runtime_error("Terminator incorrectly drawn");});
    check(!pending.busy()&&pending.state.commands==3,"Pending job did not see a CPU-written END");
}
void pixelContracts() {
    std::vector<std::uint8_t> vram(0x80000),framebuffer(0x40000);
    const auto word=[&](unsigned at,unsigned value){vram[at]=std::uint8_t(value>>8);vram[at+1]=std::uint8_t(value);};
    const auto pixel=[&](unsigned x,unsigned y){const auto i=(y*512+x)*2;return unsigned((framebuffer[i]<<8)|framebuffer[i+1]);};
    word(4,0x80);word(6,0x120);word(8,0x20);word(10,0x102);word(12,3);word(14,4);
    const std::array<std::uint8_t,8> pattern={0x01,0x23,0x45,0x6f,0xf0,0x12,0x30,0x40};std::copy(pattern.begin(),pattern.end(),vram.begin()+0x100);
    tecmo98::draw_vdp1_primitive(vram,0,{},framebuffer,false);
    check(pixel(3,4)==0&&pixel(4,4)==0x121&&pixel(10,4)==0x12f&&pixel(3,5)==0x12f,"Normal 4-bit sprite source/bank/transparency differs");
    std::fill(framebuffer.begin(),framebuffer.end(),0);word(0,0x10);tecmo98::draw_vdp1_primitive(vram,0,{},framebuffer,false);
    check(pixel(3,4)==0x12f&&pixel(9,4)==0x121&&pixel(10,4)==0,"Normal sprite horizontal flip differs");
    std::fill(framebuffer.begin(),framebuffer.end(),0);word(0,0);word(4,0);vram[0x100]=0xf1;vram[0x101]=0xf2;
    tecmo98::draw_vdp1_primitive(vram,0,{},framebuffer,false);
    check(pixel(4,4)==0x121&&pixel(6,4)==0,"Two sprite end codes did not stop the row");
    word(4,0xc0);word(12,0);word(14,0);vram[0x100]=0x12;vram[0x104]=0x34;
    std::fill(framebuffer.begin(),framebuffer.end(),0);tecmo98::draw_vdp1_primitive(vram,0,{},framebuffer,true,8);
    check(framebuffer[0]==0x21&&framebuffer[1]==0x22&&framebuffer[1024]==0,"Even interlaced line selection differs");
    std::fill(framebuffer.begin(),framebuffer.end(),0);tecmo98::draw_vdp1_primitive(vram,0,{},framebuffer,true,12);
    check(framebuffer[0]==0x23&&framebuffer[1]==0x24,"Odd interlaced line addressing differs");
    std::fill(framebuffer.begin(),framebuffer.end(),0);word(0,4);word(6,0x801f);
    const std::array<unsigned,8> points={1,1,3,1,3,3,1,3};for(unsigned i=0;i<8;++i)word(12+i*2,points[i]);
    tecmo98::draw_vdp1_primitive(vram,0,{},framebuffer,false);
    check(pixel(2,2)==0x801f&&pixel(0,0)==0&&pixel(4,4)==0,"Solid polygon native pixel coverage differs");
}
void burstAndDivisionContracts() {
    for(unsigned destinationMode=0;destinationMode<3;++destinationMode){
        auto ram=std::make_unique<tecmo98::WorkRam>();for(unsigned i=0;i<64;++i)ram->write8(0x06001000+i,std::uint8_t(0x30+i));
        auto expected=std::make_unique<tecmo98::WorkRam>(*ram);const unsigned destination=0x06002000;
        for(unsigned block=0;block<3;++block){const auto at=destinationMode==0?destination:destinationMode==1?destination+block*16:destination-(block+1)*16;for(unsigned b=0;b<16;++b)expected->write8(at+b,std::uint8_t(0x30+block*16+b));}
        std::array<std::uint32_t,4> channel={0x06001003,destination+3,12,0x1e01|(destinationMode<<14)};
        check(tecmo98::transfer_cpu_dma_high_ram(*ram,channel,1)&&ram->bytes==expected->bytes&&channel[2]==0&&(channel[3]&2)!=0,"CPU 16-byte burst contents/count/alignment differ");
    }
    tecmo98::NativeDivider division;division.write(0,5);division.write(4,std::uint32_t(-16));
    check(division.read(4)==std::uint32_t(-3)&&division.read(24)==std::uint32_t(-1),"Signed 32-bit DIVU quotient/remainder differ");
    division.write(0,3);division.write(16,1);division.write(20,16);
    check(division.read(28)==1431655770&&division.read(16)==2,"Signed 64-bit DIVU quotient/remainder differ");
    division.write(16,0xfffffffe);division.write(20,0xffffffff);
    check(division.read(28)==std::uint32_t(-1431655765)&&division.read(16)==std::uint32_t(-2),"Negative 64-bit DIVU result differs");
    division.write(0,0);division.write(4,16);check(division.read(28)==0x7fffffff&&division.read(8)==1,"DIVU positive overflow did not saturate/set OVF");
    bool partial=false;try{division.read(16);}catch(const std::exception&){partial=true;}check(partial,"DIVU fabricated the partial six-cycle remainder");
    division.write(4,std::uint32_t(-16));check(division.read(28)==0x80000000,"DIVU negative overflow did not saturate");
    division.write(0,0xffffffff);division.write(4,0x80000000);check(division.read(28)==0x7fffffff,"DIVU INT_MIN/-1 overflow differs");
}
void rtcClockContract(char** argv) {
    std::uint64_t now=0;bool slowDraw=false;auto session=std::make_unique<tecmo98::NativeSession>(argv[2],argv[3],argv[4],argv[5],[&]{const auto instant=now;if(slowDraw){slowDraw=false;now+=90000000000ull;}return instant;});
    const auto seconds=[&]{session->write(0x20100001,1,1);session->write(0x20100003,1,0);session->write(0x20100063,1,1);session->write(0x2010001f,1,0x10);now+=16000;session->read(0x20100063,1);const auto b=session->read(0x2010002f,1);return unsigned((b>>4)*10+(b&15));};
    const auto initial=seconds();now+=3000000000ull;const auto later=seconds();
    check((later+60-initial)%60==3,"SMPC RTC did not advance with the injected board clock");
    const auto beforeDraw=session->gameNanoseconds();slowDraw=true;session->composeFrame();
    check(session->gameNanoseconds()==beforeDraw&&seconds()==later,"A slow host draw advanced the board RTC");
}
void interlacedEraseContract(char** argv){
    std::uint64_t now=0;auto session=std::make_unique<tecmo98::NativeSession>(argv[2],argv[3],argv[4],argv[5],[&]{return now;});
    session->write(0x25fe00a0,4,0xffffffff);session->write(0x25d00000,2,1);session->write(0x25d00002,2,8);session->write(0x25d00006,2,0x1234);session->write(0x25d00008,2,0);session->write(0x25d0000a,2,(44<<9)|223);
    for(unsigned y:{200u,223u,224u})session->write(0x25c80000+y*1024,4,0xa5a5a5a5);
    session->read(0x25f80004,2);now=17000000;session->runCooperative(1);check(session->videoView().display[200*1024]==0xa5,"VDP1 initial field swap differs");
    now=34000000;session->runCooperative(1);const auto draw=session->videoView().draw;
    check(draw[200*1024]==0x12&&draw[223*1024+1]==0x34&&draw[224*1024]==0xa5,"VDP1 DIE erase halved framebuffer row coordinates or exceeded its range");
}
void zeroAnglePortFixContract(char** argv){
    auto session=std::make_unique<tecmo98::NativeSession>(argv[2],argv[3],argv[4],argv[5],[]{return std::uint64_t(0);});
    auto& c=session->context;c.pc=0x060583e0;c.stop=0xf0000000;c.special(0x114,c.stop);c.special(0x100,0xf0);c.reg(4,0);c.reg(5,0);c.reg(8,0xdeadbeef);
    tecmo98::aot::run(c,*session,20);check(c.pc==c.stop&&c.reg(0)==0&&c.reg(8)==0xdeadbeef&&session->zeroVectorAngleFixes()==1,"Zero-vector native angle policy/return/register preservation differ");
    for(const auto input:std::array<std::array<std::uint32_t,3>,4>{{{65536,0,0},{0,65536,0x4000},{0xffff0000,0,0x8000},{0,0xffff0000,0xc000}}}){
        c.pc=0x060583e0;c.reg(4,input[0]);c.reg(5,input[1]);tecmo98::aot::run(c,*session,200);
        check(c.pc==c.stop&&c.reg(0)==input[2]&&session->zeroVectorAngleFixes()==1,"Nonzero direction did not retain its original angle instruction path");
    }
    c.pc=0x060583e0;c.reg(4,0);c.reg(5,0);session->write(0x060583e4,2,0);bool rejected=false;try{tecmo98::aot::run(c,*session,20);}catch(const std::exception&){rejected=true;}
    check(rejected&&session->zeroVectorAngleFixes()==1,"Zero-vector native fix ignored a changed function body");
}
void timedSmpcContracts(char** argv) {
    std::uint64_t now=0;auto session=std::make_unique<tecmo98::NativeSession>(argv[2],argv[3],argv[4],argv[5],[&]{return now;});
    session->write(0x25fe00a0,4,0xffffffff);session->write(0x20100001,1,1);session->write(0x20100003,1,0);session->write(0x20100005,1,0xf0);
    session->write(0x20100063,1,1);session->write(0x2010001f,1,0x10);now=15999;
    check(session->read(0x20100063,1)==1,"SMPC status request completed before its timer");now=16000;
    check(session->read(0x20100063,1)==0&&session->read(0x20100061,1)==0x40&&session->read(0x20100033,1)==1&&session->read(0x2010005f,1)==0x10,"SMPC status/region/command completion differs");
    check((session->read(0x25fe00a4,4)&0x80)!=0,"SMPC completion interrupt missing");
    session->write(0x20100001,1,0);session->write(0x20100003,1,8);session->write(0x20100063,1,1);session->write(0x2010001f,1,0x10);
    now+=708000;check(session->read(0x20100063,1)==0&&session->read(0x20100061,1)==0xc0&&session->read(0x20100021,1)==0xf0,"Empty ST-V Saturn peripheral port response differs");
    session->write(0x20100001,1,0x80);check(session->read(0x20100063,1)==1,"SMPC continuation did not become busy");now+=700000;
    check(session->read(0x20100063,1)==0&&session->read(0x20100061,1)==0x80,"SMPC final peripheral packet flag differs");
}
void frameSnapshotContract(char** argv){
 std::uint64_t now=0;auto session=std::make_unique<tecmo98::NativeSession>(argv[2],argv[3],argv[4],argv[5],[&]{return now;});
 session->read(0x25f80004,2);unsigned callbacks=0;std::uint64_t serial=0;
 session->videoFrameReady=[&](const tecmo98::NativeVideoView& view,std::uint64_t time,std::uint64_t source){check(time==now&&source>serial&&view.display.size()==0x40000&&view.vdp2.size()==512,"Invalid coherent video boundary snapshot");++callbacks;serial=source;};
 now=20000000;session->read(0x25f80004,2);check(callbacks==1&&serial==session->videoFrames(),"Missing frame boundary snapshot");
 session->read(0x25f80004,2);check(callbacks==1,"Repeated host poll produced duplicate source frame");
 now=100000000;session->read(0x25f80004,2);check(callbacks==2&&serial>=5&&serial==session->videoFrames(),"Delayed host poll queued old snapshots instead of only latest frame");
}
void integration(char** argv) {
    auto session=std::make_unique<tecmo98::NativeSession>(argv[2],argv[3],argv[4],argv[5],[]{return std::uint64_t(0);});
    const auto config=session->read(0xffffffe0,4);session->write(0xffffffe0,4,0x1234ffff);
    check(session->read(0xffffffe0,4)==config,"BCR changed without unlock key");
    session->write(0xffffffe0,4,0xa55affff);check(session->read(0xffffffe0,4)==0x1ff7,"BCR read mask/key mismatch");
    session->write(0x06080000,4,0x12345678);check(session->read(0x26080000,4)==0x12345678,"Native RAM alias mismatch");
    session->write(0x46080000,4,0);check(session->read(0x06080000,4)==0x12345678,"Cache purge corrupted shared RAM");
    session->write(0xffffffb0,4,0);
    for(unsigned i=0;i<4;++i)session->write(0x06080000+i*4,4,0x12340000+i);
    session->write(0xffffff80,4,0x26080000);session->write(0xffffff84,4,0x25e00400);session->write(0xffffff88,4,4);session->write(0xffffff8c,4,0x5a01);
    check(session->read(0x25e00400,4)==0,"DMA ran with global enable off");session->write(0xffffffb0,4,1);
    for(unsigned i=0;i<4;++i)check(session->read(0x25e00400+i*4,4)==0x12340000+i,"Native RAM-to-video DMA data mismatch");
    check(session->read(0xffffff88,4)==0&&(session->read(0xffffff8c,4)&3)==3,"DMA completion flags missing");
    session->write(0xfffffe11,1,0x80);check(session->read(0xfffffe11,1)==0,"Software raised hardware input-capture flag");
    session->write(0x21800000,2,0);check((session->read(0xfffffe11,1)&0x80)!=0,"Peer pulse did not latch input capture");
    session->write(0xfffffe11,1,0);check((session->read(0xfffffe11,1)&0x80)==0,"Capture flag did not clear");
    // Original RTE at 0x06002052 restores a delivered native interrupt's ABI frame.
    auto& c=session->context;c.pc=0x06004108;c.special(0x100,0);c.reg(15,0x060ff000);
    session->write(0x06000190,4,0x06002052);session->write(0x21800000,2,0);session->runCooperative(1);
    check(c.pc==0x06004108&&c.special(0x100)==0&&c.reg(15)==0x060ff000,"Native capture interrupt/RTE restoration failed");
    check(session->read(0x060feff8,4)==0x06004108&&session->read(0x060feffc,4)==0,"Native interrupt saved frame differs");
    check(session->read(0x20400001,1)==0xff,"Idle arcade input asserted contacts");
    session->setInputContacts(0,0xa5);check(session->read(0x20400001,1)==0x5a&&session->read(0x20400021,1)==0x5a,"Arcade simultaneous contacts or mirror differ");
    session->write(0x20400007,1,0x39);check(session->read(0x20400007,1)==0x39,"Coin/output latch did not read back");
    session->write(0x20180000,4,0x12345678);check(session->read(0x20180000,4)==0x00340078,"Backup RAM odd-lane mapping differs");
    // Audio output latch must not accidentally assert test/service input pins.
    session->write(0x2010007b,1,0x18);session->write(0x20100077,1,0x10);
    check(session->read(0x20100077,1)==0x77,"SMPC pin direction/input readback differs");
    session->write(0x2010007b,1,0);check(session->read(0x20100077,1)==0x7f,"SMPC input pullups disappeared");
    session->write(0x25fe00a0,4,0xffffffff);
    for(unsigned i=0;i<16;++i)session->write(0x06080000+i,1,0xa0+i);
    session->write(0x25fe0000,4,0x06080000);session->write(0x25fe0004,4,0x05e01000);
    session->write(0x25fe0008,4,16);session->write(0x25fe000c,4,0x101);session->write(0x25fe0014,4,7);
    session->write(0x25fe0010,4,0x101);
    for(unsigned i=0;i<16;++i)check(session->read(0x05e01000+i,1)==0xa0+i,"SCU DMA did not deliver ordered halfwords to VRAM");
    check(session->read(0x25fe0000,4)==0x06080000&&session->read(0x25fe0004,4)==0x05e01000&&session->read(0x25fe0010,4)==0,"SCU register restoration/readback differs");
    check((session->read(0x25fe00a4,4)&0x800)!=0,"SCU DMA completion interrupt missing after transfer");
    session->write(0x25fe0000,4,0x06080001);session->write(0x25fe0004,4,0x06081000);
    session->write(0x25fe0008,4,8);session->write(0x25fe000c,4,0x107);session->write(0x25fe0014,4,0x10107);
    session->write(0x25fe0010,4,0x101);
    for(unsigned i=0;i<8;++i)check(session->read(0x06081000+i,1)==0xa1+i,"SCU odd source rotation or Work RAM fixed destination stride differs");
    check(session->read(0x25fe0000,4)==0x06080009&&session->read(0x25fe0004,4)==0x06081008,"SCU update-address bits ignored");
    session->write(0x06082000,4,8);session->write(0x06082004,4,0x05e01200);session->write(0x06082008,4,0x06080000);
    session->write(0x0608200c,4,8);session->write(0x06082010,4,0x05e01208);session->write(0x06082014,4,0x86080008);
    session->write(0x25fe0044,4,0x06082000);session->write(0x25fe004c,4,0x101);session->write(0x25fe0054,4,0x1000007);session->write(0x25fe0050,4,0x101);
    for(unsigned i=0;i<16;++i)check(session->read(0x05e01200+i,1)==0xa0+i,"SCU descriptor transfers did not concatenate actual source data");
    check(session->read(0x25fe0044,4)==0x06082000&&(session->read(0x25fe00a4,4)&0x200)!=0,"SCU indirect job completion/configuration differs");
}
}
int main(int argc,char** argv){
    try{
        if(argc!=6){std::cerr<<"Usage: reset-tail-vectors.bin EPROM raw-BIOS cartridge profile\n";return 2;}
        const auto original=originalCopies(argv[1]),strides=addressing();failureContracts();videoContracts();pixelContracts();burstAndDivisionContracts();integration(argv);timedSmpcContracts(argv);rtcClockContract(argv);interlacedEraseContract(argv);zeroAnglePortFixContract(argv);presentationContracts();schedulerContracts(argv);macArchitectureContracts(argv);renderPublicationContract(argv);frameSnapshotContract(argv);
        std::cout<<"{\"passed\":true,\"independent_original_copy_cases\":"<<original<<",\"dma_addressing_cases\":"<<strides
                 <<",\"cpu_burst_cases\":3,\"video_contracts\":true,\"sprite_and_polygon_pixels\":true,\"smpc_timed_packets\":true,\"rtc_board_clock\":true,\"interlaced_erase_coordinates\":true,\"zero_vector_port_fix\":true,\"scu_indirect_data_transfer\":true,\"dma_rejection_preserves_state\":true,\"native_ram_to_video_dma\":true,\"capture_interrupt_rte_verified\":true,\"cache_purge_preserves_shared_data\":true,\"playable_game\":false}\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}

