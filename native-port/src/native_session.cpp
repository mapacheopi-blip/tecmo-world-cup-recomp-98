#include "native_session.hpp"
#include "native_dma.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>

namespace {
std::string at(const std::string& what,std::uint32_t address) {std::ostringstream out;out<<what<<" at 0x"<<std::hex<<address;return out.str();}
std::uint32_t physical(std::uint32_t address) {
    auto mapped=address<0x40000000?address&~0x20000000u:address;
    if(mapped>=0x06000000&&mapped<0x08000000)mapped=0x06000000|(mapped&0xfffff);
    if(mapped>=0x00200000&&mapped<0x00400000)mapped=0x00200000|(mapped&0xfffff);
    return mapped;
}
bool resetConfiguration(std::uint32_t address) {
    const auto offset=address-0xfffffe00;
    return (offset>=0x10&&offset<=0x17)||(offset>=0x60&&offset<=0x69)||offset==0x71||offset==0x72||(offset>=0xe0&&offset<=0xe5)||
        (offset>=0x1a8&&offset<=0x1ab)||(offset>=0x1a0&&offset<=0x1a3)||(offset>=0x10c&&offset<=0x10f);
}
}
namespace tecmo98 {
std::uint64_t NativeSession::SoundBus::read(std::uint32_t raw,unsigned size){const auto address=raw&0xffffff;if(address<0x100000)return owner_.read(0x05a00000+address,size);if(address>=0x100000&&address+size<=0x101000)return owner_.read(0x05b00000+address-0x100000,size);throw std::runtime_error(at("Native sound data read pending",raw));}
void NativeSession::SoundBus::write(std::uint32_t raw,unsigned size,std::uint64_t value){const auto address=raw&0xffffff;if(address<0x100000){owner_.write(0x05a00000+address,size,value);return;}if(address>=0x100000&&address+size<=0x101000){owner_.write(0x05b00000+address-0x100000,size,value);return;}throw std::runtime_error(at("Native sound data write pending",raw));}
void NativeSession::SoundBus::verifySource(std::uint32_t address,std::uint32_t expected,unsigned size){if(owner_.soundSourceTrace)owner_.soundSourceTrace(owner_.soundContext.pc);if(size==0||size>4||std::uint64_t(address)+size>owner_.soundRam_.size())throw std::runtime_error("Sound source guard outside driver RAM");std::uint32_t actual=0;for(unsigned i=0;i<size;++i)actual=(actual<<8)|owner_.soundRam_[address+i];if(actual!=expected)throw std::runtime_error(at("Sound driver source changed; retranslation required",address));}
namespace {
void soundStatus(aot_sound::Context& c,std::uint16_t sr){aot::setBe<2>(c.abi,0x200,sr);c.abi[0x40]=(sr>>15)&1;c.abi[0x41]=(sr>>13)&1;c.abi[0x42]=(sr>>8)&7;for(unsigned i=0;i<5;++i)c.abi[0x43+i]=(sr>>(4-i))&1;}
std::uint16_t soundStatus(const aot_sound::Context& c){unsigned sr=(c.abi[0x40]<<15)|(c.abi[0x41]<<13)|(c.abi[0x42]<<8);for(unsigned i=0;i<5;++i)sr|=c.abi[0x43+i]<<(4-i);return std::uint16_t(sr);}
}
void NativeSession::resetSoundPort(std::uint8_t output){
    platform_.writeAudioPort2(output);
    if((output&0x10)!=0){soundHeld_=true;return;}
    if(soundHeld_){
        const auto alreadyExecuted=soundContext.executed;
        soundContext=aot_sound::Context{};soundContext.executed=alreadyExecuted;soundContext.pc=std::uint32_t(soundBus_.read(4,4));soundContext.stop=0x1580;
        const auto stack=std::uint32_t(soundBus_.read(0,4));if(soundContext.pc!=0x1000||stack!=0xa000||soundBus_.read(0x5d6c,4)!=0x53454741)throw std::runtime_error("Native sound reset vector/driver signature differs");
        aot::setBe<4>(soundContext.abi,60,stack);aot::setBe<4>(soundContext.abi,0x100,stack);soundStatus(soundContext,0x2700);soundHeld_=false;
    }
}
void NativeSession::runSound(std::uint64_t instructions){
    deliverSoundInterrupt();
    const auto initial=soundContext.executed;
    while(soundContext.executed-initial<instructions){
        if(soundContext.pc==0x1580){
            // The family Sleigh model emits RETURN(0) for RTE. Restore the actual
            // six-byte 68000 frame here, with an original-byte guard.
            soundBus_.verifySource(0x1580,0x4e73,2);const auto sp=std::uint32_t(aot::be<4>(soundContext.abi,60));
            const auto sr=std::uint16_t(soundBus_.read(sp,2));if((sr&0x2000)==0)throw std::runtime_error("68000 user-stack exception return pending");
            soundContext.pc=std::uint32_t(soundBus_.read(sp+2,4));aot::setBe<4>(soundContext.abi,60,sp+6);soundStatus(soundContext,sr);++soundContext.executed;
        }else aot_sound::run(soundContext,soundBus_,instructions-(soundContext.executed-initial));
    }
}
void NativeSession::deliverSoundInterrupt(){
    scsp_.poll(now_());const auto level=scsp_.irqLevel();if(level<=soundContext.abi[0x42])return;
    auto stack=std::uint32_t(aot::be<4>(soundContext.abi,60));const auto status=soundStatus(soundContext);
    if((status&0x2000)==0)throw std::runtime_error("68000 user-stack interrupt entry pending");
    stack-=4;soundBus_.write(stack,4,soundContext.pc);stack-=2;soundBus_.write(stack,2,status);
    aot::setBe<4>(soundContext.abi,60,stack);soundStatus(soundContext,std::uint16_t((status&~0x8700u)|(level<<8)));
    soundContext.pc=std::uint32_t(soundBus_.read((24+level)*4,4));
}
NativeSession::NativeSession(const std::filesystem::path& eprom,const std::filesystem::path& bios,
                            const std::filesystem::path& cart,const std::filesystem::path& profile,std::function<std::uint64_t()> timeSource)
    :bios_(load_jp1_bios(bios)),cart_(load_cartridge(cart)),ram_(std::make_unique<WorkRam>()),
     lowRam_(0x100000),video_(video_storage_size),vdp2Vram_(0x80000),colorRam_(0x1000) {
    scsp_.attachRam(soundRam_);
    crypt_=std::make_unique<NativeCrypt5881>(0x05200913,[this](std::uint32_t word){
        const auto address=0x02000000ull+std::uint64_t(word)*2;if(address+2>0x05000000)throw std::out_of_range("Encrypted cartridge source outside mapped image");
        const auto bytes=cart_.span(std::uint32_t(address),2);return std::uint16_t((bytes[1]<<8)|bytes[0]);
    });
    load_program(*ram_,eprom);
    const auto resident=load_resident_profile(profile);
    std::copy_n(resident.begin(),0x4000,ram_->bytes.begin());
    clear_06004002(*ram_,video_);
    begin_reset_000034e8(*ram_,platform_);continue_reset_00003504(*ram_,platform_);
    continue_reset_00003510(*ram_,bios_,code_,onChip_);
    context.pc=0x06004108;context.special(0x104,0x060ffc00);context.special(0x100,0xf0);
    context.special(0x108,0x06000000);context.reg(15,ram_->read32(0x06064e7c));
    const auto epoch=std::chrono::steady_clock::now();
    hostNow_=timeSource?std::move(timeSource):std::function<std::uint64_t()>([epoch]{return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-epoch).count());});
    now_=[this]{return (hostPauseDepth_!=0?hostPauseStarted_:hostNow_())-excludedHostWork_;};
    const auto now=now_();rtcEpoch_=std::time(nullptr);rtcGameEpoch_=now;frcEpoch_={now,now};raster_.configure(0,ram_->read32(0x06000324)!=0,now);
}
NativeFrame NativeSession::composeFrame(){HostWorkScope scope(*this);return compose_native_frame(videoView());}
void NativeSession::postScu(unsigned source) {
    const auto bit=1u<<source;
    if((platform_.interruptMask&bit)!=0)platform_.scu.words[0xa4/4]|=bit;else pendingScu_|=bit;
}
void NativeSession::triggerScuDma(unsigned event) {
    for(unsigned level=0;level<3;++level){const auto base=level*8;if((platform_.scu.words[base+4]&0x100)!=0&&(platform_.scu.words[base+5]&7)==event)runScuDma(level);}
}
void NativeSession::runScuDma(unsigned level,bool notify) {
    HostCostScope hostWork(*this,2);
    auto* cfg=platform_.scu.words.data()+level*8;
    if((cfg[5]&0x1000000)!=0){
        struct Transfer{std::uint32_t source,destination,count;};std::vector<Transfer> transfers;
        auto table=cfg[1];bool ended=false;
        for(unsigned job=0;job<4096;++job){
            validateRange(table,12,false);const auto count=std::uint32_t(read(table,4))&(level==0?0xfffff:0x3ffff);
            const auto destination=std::uint32_t(read(table+4,4))&0x07ffffff,source=std::uint32_t(read(table+8,4));
            transfers.push_back({source&0x07ffffff,destination,count!=0?count:level==0?0x100000u:0x2000u});table+=12;
            if((source&0x80000000)!=0){ended=true;break;}
        }
        if(!ended)throw std::runtime_error("SCU indirect DMA has no bounded descriptor terminator");
        for(const auto& transfer:transfers)if((transfer.source&0x07f00000)==0){postScu(12);return;}
        const auto saved=std::array<std::uint32_t,6>{cfg[0],cfg[1],cfg[2],cfg[3],cfg[4],cfg[5]};
        // Ordered descriptor transfers share the same native memory adapter. Inner
        // completion is held until the complete descriptor job has finished.
        for(const auto& transfer:transfers){
            const auto bytes=std::uint64_t(transfer.count)*std::max(2u,1u<<(saved[3]&7))/2;
            if(transfer.destination<table&&std::uint64_t(transfer.destination)+bytes>saved[1])throw std::runtime_error("SCU DMA overlapping its descriptor table needs validation");
            cfg[0]=transfer.source;cfg[1]=transfer.destination;cfg[2]=transfer.count;cfg[5]=saved[5]&~0x1000000u;
            try{runScuDma(level,false);}catch(...){std::copy(saved.begin(),saved.end(),cfg);throw;}
        }
        std::copy(saved.begin(),saved.end(),cfg);if(notify)postScu(11-level);return;
    }
    if((cfg[0]&0x07f00000)==0){postScu(12);return;} // BIOS bus is not connected to SCU DMA.
    const auto count=cfg[2]!=0?cfg[2]:level==0?0x100000u:0x1000u;
    if((count&1)!=0)throw std::runtime_error("Odd SCU DMA byte count needs validation");
    const unsigned sourceAdd=(cfg[3]&0x100)!=0?4:0,destinationAdd=(cfg[3]&7)==0?0:1u<<(cfg[3]&7);
    auto source=cfg[0],destination=cfg[1];unsigned half=((source&2)>>1)^1;
    const auto advance=[&]{half^=1;if(half!=0)source+=sourceAdd;destination+=(destination&0x07000000)==0x06000000?2:destinationAdd;};
    // Preflight every actual halfword including stride and source-byte rotation.
    for(unsigned i=0;i<count;i+=2){validateRange((source&~3u)+(half==0?2:0)+(source&1),2,false);validateRange(destination,2,true);advance();}
    source=cfg[0];destination=cfg[1];half=((source&2)>>1)^1;
    for(unsigned i=0;i<count;i+=2){write(destination,2,read((source&~3u)+(half==0?2:0)+(source&1),2));advance();}
    if((cfg[5]&0x10000)!=0)cfg[0]=source;if((cfg[5]&0x100)!=0)cfg[1]=destination;
    if(notify)postScu(11-level); // Completion follows the real native memory transfer.
}
void NativeSession::pollSmpc() {
    if(!intbackPending_||now_()<intbackDue_)return;
    intbackPending_=false;smpcRequestArmed_=false;smpcCompletion_=0x10;
    if(intbackContinuation_||(intbackRequest_[0]==0&&(intbackRequest_[1]&8)!=0)){
        // ST-V has no Saturn-pad connectors; host game inputs use the JAMMA IO IC.
        smpcOutput_.fill(0xff);smpcOutput_[0]=0xf0;smpcOutput_[1]=0xf0;
        smpcStatus_=std::uint8_t((intbackStage_==2?0x80:0xc0)|(intbackRequest_[0]>>4));
        intbackStage_=intbackStage_==2?0:2;postScu(7);return;
    }
    if(intbackRequest_[0]!=0){
        smpcOutput_.fill(0xff);smpcOutput_[0]=std::uint8_t(resetControlBit_<<6);
        // RTC advances with the board clock, including native presentation pauses.
        // A wall-clock read here skips seconds during a slow software draw and
        // the original RTC watchdog at 06036b78 then wipes both work RAMs.
        const auto instant=rtcEpoch_+std::time_t((now_()-rtcGameEpoch_)/1000000000ull);std::tm calendar{};if(localtime_s(&calendar,&instant)!=0)throw std::runtime_error("Native calendar unavailable");
        const auto bcd=[](int n){return std::uint8_t((n/10)*16+n%10);};const auto year=calendar.tm_year+1900;
        smpcOutput_[1]=bcd(year/100);smpcOutput_[2]=bcd(year%100);smpcOutput_[3]=std::uint8_t((calendar.tm_wday<<4)|(calendar.tm_mon+1));
        smpcOutput_[4]=bcd(calendar.tm_mday);smpcOutput_[5]=bcd(calendar.tm_hour);smpcOutput_[6]=bcd(calendar.tm_min);smpcOutput_[7]=bcd(calendar.tm_sec);
        smpcOutput_[8]=0;smpcOutput_[9]=1;smpcOutput_[10]=std::uint8_t(0x34|(ram_->read32(0x06000324)!=0?0x40:0));smpcOutput_[11]=0;
        for(unsigned i=12;i<16;++i)smpcOutput_[i]=0;
        intbackStage_=(intbackRequest_[1]&8)!=0?1:0;smpcStatus_=std::uint8_t(0x40|(intbackStage_!=0?0x20:0));postScu(7);
    }
}
std::uint16_t NativeSession::counter(unsigned cpu) {
    auto& chip=cpu==0?onChip_:slaveOnChip_;const auto mode=chip.requested[0x16]&3;
    if(mode==3)throw std::runtime_error("External free-running timer clock source pending");
    const unsigned divisor=mode==0?8:mode==1?32:128;
    const std::uint64_t frequency=(ram_->read32(0x06000324)!=0?28636362:26846589)/divisor;
    const auto elapsed=now_()-frcEpoch_[cpu],ticks=(elapsed/1000000000)*frequency+((elapsed%1000000000)*frequency)/1000000000;
    const auto previous=std::uint16_t(frcBase_[cpu]+frcLastTicks_[cpu]),current=std::uint16_t(frcBase_[cpu]+ticks);
    const auto advanced=ticks-frcLastTicks_[cpu];const auto& compare=cpu==0?timerCompare_:slaveTimerCompare_;
    const auto crosses=[&](std::uint16_t target){const auto distance=std::uint16_t(target-previous);return advanced>=(distance==0?65536u:unsigned(distance));};
    if(advanced!=0){if(crosses(0))chip.requested[0x11]|=2;if(crosses(compare[0]))chip.requested[0x11]|=8;if(crosses(compare[1]))chip.requested[0x11]|=4;}
    frcLastTicks_[cpu]=ticks;return current;
}
void NativeSession::submitControlDraw() {
    if((vdp1Configuration_[1]&7)>1||((vdp1Configuration_[3]&4)!=0&&(vdp1Configuration_[3]&8)==0))throw std::runtime_error("VDP1 rotated/reserved framebuffer mode pending");
    gpuStatus_&=~2u;
    gpuLast_=gpuJob_.state.lastCommand;gpuJob_.begin();gpuEpoch_=now_();gpuTicks_=0;
}
void NativeSession::pollGpu() {
    if(!gpuJob_.busy())return;
    const auto elapsed=now_()-gpuEpoch_;
    const std::uint64_t frequency=28636362/16; // Initial command scheduling budget; pixel timings are not cycle-accurate.
    const auto ticks=(elapsed/1000000000)*frequency+((elapsed%1000000000)*frequency)/1000000000;
    const auto steps=std::min<std::uint64_t>(ticks-gpuTicks_,16384);gpuTicks_+=steps;
    {HostCostScope hostWork(*this,1);gpuJob_.step(std::span(video_).first(0x80000),unsigned(steps),[&](unsigned position,const CommandListState& state){draw_vdp1_primitive(std::span(video_).first(0x80000),position,state,std::span(video_).subspan(0x80000,0x40000),(vdp1Configuration_[1]&1)!=0,vdp1Configuration_[3]);});}
    gpuState_=gpuJob_.state;
    if(!gpuJob_.busy()){gpuStatus_|=2;++drawLists_;postScu(13);triggerScuDma(6);}
}
void NativeSession::eraseFramebuffer(std::span<std::uint8_t> target) {
    if((vdp1Configuration_[1]&7)>1)throw std::runtime_error("VDP1 rotated erase geometry pending");
    const auto word=[&](unsigned at){return unsigned((vdp1Configuration_[at]<<8)|vdp1Configuration_[at+1]);};
    const auto upper=word(8),lower=word(10),color=word(6);
    const auto x1=((upper>>9)&0x3f)*16,y1=upper&0x1ff;
    const auto x2=std::min(1024u,((lower>>9)&0x7f)*16),y2=std::min(256u,(lower&0x1ff)+1);
    // Coordinates are eight 16-bit words; in 8-bit mode each word holds two dots.
    for(unsigned y=y1;y<y2;++y)for(unsigned x=x1;x<x2;++x)target[y*1024+x]=std::uint8_t(color>>((x&1)==0?8:0));
}
void NativeSession::frameBoundary() {
    ++videoFrames_;
    gpuStatus_=std::uint16_t((gpuStatus_&2)|((gpuStatus_>>1)&1));
    const auto mode=vdp1Configuration_[3]&3;bool changed=false;
    if(eraseNextFrame_&&mode==3&&framebufferAccessed_){eraseFramebuffer(displayFramebuffer_);eraseNextFrame_=false;}
    if(mode==0||(mode==3&&framebufferAccessed_)){
        std::swap_ranges(displayFramebuffer_.begin(),displayFramebuffer_.end(),video_.begin()+0x80000);gpuStatus_&=~2u;changed=true;
        if(mode==0||(vdp1Configuration_[1]&8)!=0)eraseFramebuffer(std::span(video_).subspan(0x80000,0x40000));
    }
    if(mode==2&&framebufferAccessed_)eraseNextFrame_=true;
    framebufferAccessed_=false;
    if(changed&&vdp1Configuration_[5]==2)submitControlDraw();
}
RasterSample NativeSession::pollVideo() {
    const auto previousVideoFrame=videoFrames_;
    pollGpu();
    const auto mode=std::uint16_t((vdp2Configuration_[0]<<8)|vdp2Configuration_[1]);
    raster_.configure(mode,ram_->read32(0x06000324)!=0,now_());const auto sample=raster_.sample(now_());
    if(rasterGeneration_!=raster_.generation()){rasterGeneration_=raster_.generation();lastRasterTicks_=sample.ticks;return sample;}
    for(auto line=lastRasterTicks_/sample.totalWidth;line<=sample.absoluteLine;++line){
        const auto start=line*sample.totalWidth;const auto y=line%sample.totalHeight;
        if(start>lastRasterTicks_&&start<=sample.ticks){
            if(y==0){postScu(1);triggerScuDma(1);frameBoundary();}
            if(y==sample.height){postScu(0);triggerScuDma(0);}
        }
        const auto hblank=start+sample.width;
        if(y<sample.height&&hblank>lastRasterTicks_&&hblank<=sample.ticks){postScu(2);triggerScuDma(2);}
    }
    lastRasterTicks_=sample.ticks;if(videoFrames_!=previousVideoFrame&&videoFrameReady)videoFrameReady(videoView(),now_(),videoFrames_);return sample;
}
std::uint16_t NativeSession::readVideoWord(std::uint32_t address) {
    const auto sample=pollVideo();
    if(address==0x05f80002){if((vdp2Configuration_[2]&2)==0){latchedH_=sample.hcounter;latchedV_=sample.vcounter;externalLatchFlag_=true;}}
    if(address==0x05f80004){const auto status=std::uint16_t(sample.status|(externalLatchFlag_?0x200:0));externalLatchFlag_=false;return status;}
    if(address==0x05f80008)return latchedH_;if(address==0x05f8000a)return latchedV_;
    return std::uint16_t((vdp2Configuration_[address-0x05f80000]<<8)|vdp2Configuration_[address-0x05f80000+1]);
}
void NativeSession::startSlave() {
    if(platform_.slave.resetHeld()) {
        const auto alreadyExecuted=slaveContext.executed;
        slaveContext=aot::Context{};slaveContext.executed=alreadyExecuted;slaveContext.pc=bios_.read32(0);
        slaveContext.reg(15,bios_.read32(4));slaveContext.special(0x100,0xf0);
        slaveOnChip_=OnChipResetConfiguration{};slaveCode_=NativeCodePolicy{};
        slaveDma_=CpuDmaDispatchGate{};slaveBusConfiguration_=NativeBusConfiguration{};
        slaveDmaChannels_={};
        slaveTimerCompare_={};
        divider_[1]=NativeDivider{};
        frcBase_[1]=0;frcCapture_[1]=0;frcEpoch_[1]=now_();frcLastTicks_[1]=0;
    }
    platform_.slave.releaseReset();
}
void NativeSession::deliverCaptureInterrupt() {
    auto& c=activeSlave_?slaveContext:context;const auto& chip=activeSlave_?slaveOnChip_:onChip_;
    counter(activeSlave_?1:0);
    const auto status=c.special(0x100);unsigned level=0,vector=0;int scuSource=-1;
    const auto flags=chip.requested[0x10]&chip.requested[0x11]&0x8e;
    if(flags!=0){level=chip.requested[0x60]&15;vector=(flags&0x80)!=0?chip.requested[0x66]&0x7f:(flags&12)!=0?chip.requested[0x67]&0x7f:chip.requested[0x68]&0x7f;}
    if(!activeSlave_){
        constexpr unsigned levels[14]={15,14,13,12,11,10,9,8,8,6,6,5,3,2};
        const auto pending=(pendingScu_|platform_.scu.words[0xa4/4])&~platform_.interruptMask;
        for(unsigned i=0;i<14;++i)if((pending&(1u<<i))!=0&&levels[i]>level){level=levels[i];vector=0x40+i;scuSource=int(i);}
    }
    if(level<=((status>>4)&15))return;
    const auto destination=std::uint32_t(read(c.special(0x108)+vector*4,4));
    if(interruptTrace)interruptTrace(activeSlave_,scuSource,vector,destination,c.pc);
    auto stack=c.reg(15);ram_->offset(stack-8,8);
    stack-=4;write(stack,4,status);stack-=4;write(stack,4,c.pc);
    c.reg(15,stack);c.special(0x100,(status&~0xf0u)|(level<<4));c.pc=destination;
    if(scuSource>=0){pendingScu_&=~(1u<<scuSource);platform_.scu.words[0xa4/4]&=~(1u<<scuSource);}
}
bool NativeSession::fixZeroVectorAngle(aot::Context& c){
    if(physical(c.pc)!=0x060583e0||c.reg(4)!=0||c.reg(5)!=0)return false;
    // Explicit game-port bug fix, not a DIVU/open-bus approximation. The game
    // calls slAtan(0,0) from 060236aa when a player has no direction vector.
    // Its original 0/0 then indexes 86058492. Define the formerly undefined
    // direction as zero. Preserve the original nonzero instruction path.
    constexpr std::array<std::uint32_t,45> original={0x44118d02u,0xe100644bu,0xe1044511u,0x8d02e3ffu,0x655b7108u,0x34538d04u,0x4318254au,0x245a254au,0x71104509u,0x45011340u,0x6459644fu,0x13444528u,0x1355e440u,0x4418c71fu,0x6203c716u,0x001e6543u,0x355ce1feu,0x402b5037u,0x2019000bu,0x002d2019u,0x002d3508u,0x000b6053u,0x2019002du,0x600b000bu,0x600d2019u,0x002d000bu,0x305c2019u,0x002d3408u,0x000b6043u,0x2019002du,0x000b304cu,0x2019002du,0x354c000bu,0x305c2019u,0x002d354cu,0x3508000bu,0x60530009u,0x06058428u,0x0605842eu,0x06058438u,0x06058442u,0x0605844au,0x06058454u,0x0605845cu,0x06058466u};
    verifySource(0x060583e0,original[0],4);
    for(unsigned i=1;i<original.size();++i)if(ram_->read32(0x060583e0+i*4)!=original[i])throw std::runtime_error("Zero-vector angle fix source signature differs");
    c.reg(0,0);c.reg(1,0xfffffffe);c.reg(2,0x06058494);c.reg(3,0xffffff00);c.reg(4,0x4000);c.reg(5,0x8000);
    c.special(0x100,c.special(0x100)|1);c.pc=c.special(0x114);--c.remaining;++c.executed;++zeroVectorAngleFixes_;return true;
}
void NativeSession::runCooperative(std::uint64_t maximumInstructions) {
    const auto total=[&]{return context.executed+slaveContext.executed+soundContext.executed;};const auto initial=total();
    // ST-V's native command producer/consumer share buffers. Keep short
    // cooperative slices: the reference also requests a 5us interleave for
    // twcup98 communication (stv.cpp init_twcup98). Instruction counts here
    // are scheduling budgets, not a claim of SH-2 cycle accuracy.
    const auto advance=[&]{schedulerPhase_=renderQueuePublicationPending_?1:(schedulerPhase_+1)%3;schedulerQuantumLeft_=schedulerPhase_==2?24:64;};
    while(total()-initial<maximumInstructions){
        activeSound_=schedulerPhase_==2;activeSlave_=schedulerPhase_==1;
        pollSmpc();pollVideo();scsp_.poll(now_());if(scsp_.mainIrq()&&!soundMainIrq_)postScu(6);soundMainIrq_=scsp_.mainIrq();
        if(activeSlave_&&platform_.slave.resetHeld()&&renderQueuePublicationPending_)throw std::runtime_error("Native render publication requires a running worker");
        if((activeSlave_&&platform_.slave.resetHeld())||(activeSound_&&soundHeld_)){advance();continue;}
        const auto budget=std::min(maximumInstructions-(total()-initial),schedulerQuantumLeft_),before=total();
        bool yielded=false;
        if(activeSound_)runSound(budget);
        else {auto& c=activeSlave_?slaveContext:context;if(c.pc==c.stop)throw std::runtime_error("Native CPU returned to its stop sentinel");c.yieldRequested=false;deliverCaptureInterrupt();executingNative_=true;try{aot::run(c,*this,budget);}catch(...){executingNative_=false;throw;}executingNative_=false;yielded=c.yieldRequested;}
        const auto spent=total()-before;if(spent>schedulerQuantumLeft_)throw std::runtime_error("Native scheduler exceeded its quantum");schedulerQuantumLeft_-=spent;
        // Preserve both the next processor and a partially consumed quantum
        // across presentation calls. Restarting each small budget on main
        // gives main four slices but slave only three for an 8192 budget.
        if(schedulerQuantumLeft_==0||yielded||spent==0)advance();
    }
}
std::uint64_t NativeSession::readByte(std::uint32_t raw) {
    const auto address=physical(raw);
    if(address<0x80000)return bios_.span(address,1)[0];
    if(address>=0x00180000&&address<0x00190000)return (address&1)!=0?backupRam_[(address-0x00180000)/2]:0;
    if(address>=0x00200000&&address<0x00300000)return lowRam_[address-0x00200000];
    if(address>=0x00400000&&address<0x00400040){const auto value=std::uint8_t((address&1)!=0?arcadeIo_.read(address-0x00400000):0xff);if(ioReadTrace)ioReadTrace(address,value);return value;}
    if(address>=0x02000000&&address<0x05000000)return cart_.span(address,1)[0];
    if(address>=WorkRam::base&&address<WorkRam::base+WorkRam::size)return ram_->read8(address);
    if(address>=0x05a00000&&address<0x05b00000)return soundRam_[address-0x05a00000];
    if(address>=0x05c00000&&address<0x05cc0000)return video_[address-0x05c00000];
    if(address>=0x05e00000&&address<0x05f00000)return vdp2Vram_[(address-0x05e00000)&0x7ffff];
    if(address>=0x05f00000&&address<0x05f80000)return colorRam_[(address-0x05f00000)&0xfff];
    if(address==0x05d00002||address==0x05d00003)return 0;
    if(address>=0x05d00000&&address<0x05d0000c)return vdp1Configuration_[address-0x05d00000];
    if(address>=0x05d00010&&address<=0x05d00017) {
        const auto at=address&~1u;std::uint16_t value=0;
        if(at==0x05d00010)value=gpuStatus_;else if(at==0x05d00012)value=gpuLast_;
        else if(at==0x05d00014)value=gpuState_.lastCommand;
        else value=std::uint16_t(0x1000|((vdp1Configuration_[5]>>1)<<8)|((vdp1Configuration_[3]&0x1c)<<3)|((vdp1Configuration_[3]&1)<<4)|(vdp1Configuration_[1]&15));
        return std::uint8_t(value>>((address&1)==0?8:0));
    }
    if(address>=0x05f80000&&address<0x05f80200) {
        if(address==0x05f80006)return vdp2Configuration_[6]&0x80;
        if(address==0x05f80007)return 0; // Version readback in the documented reference profile.
        if((address>=0x05f80002&&address<0x05f80006)||(address>=0x05f80008&&address<0x05f8000c))return std::uint8_t(readVideoWord(address&~1u)>>((address&1)==0?8:0));
        return vdp2Configuration_[address-0x05f80000];
    }
    if(address==0x00100063){pollSmpc();return platform_.systemCommandBusy||smpcRequestArmed_?1:0;}
    if(address==0x00100061)return smpcStatus_;
    if(address>=0x00100020&&address<0x0010005f)return (address&1)!=0?smpcOutput_[(address-0x00100020)/2]:0xff;
    if(address==0x0010005f)return smpcCompletion_;
    if(address==0x00100075)return std::uint8_t((0x7f&~portDirection_[0])|portReadback_[0]);
    if(address==0x00100077)return std::uint8_t((0x7f&~portDirection_[1])|portReadback_[1]);
    if(address==0xfffffe92)return (activeSlave_?slaveCode_:code_).originalControl&~0x30;
    if(address==0xfffffe11){counter(activeSlave_?1:0);return (activeSlave_?slaveOnChip_:onChip_).requested[0x11];}
    if(address==0xfffffe12||address==0xfffffe13)return std::uint8_t(counter(activeSlave_?1:0)>>((address&1)==0?8:0));
    if(address==0xfffffe18||address==0xfffffe19)return std::uint8_t(frcCapture_[activeSlave_?1:0]>>((address&1)==0?8:0));
    if(address==0xfffffe14||address==0xfffffe15) {
        const auto& chip=activeSlave_?slaveOnChip_:onChip_;
        const auto value=(activeSlave_?slaveTimerCompare_:timerCompare_)[(chip.requested[0x17]&0x10)!=0];
        return std::uint8_t(value>>((address&1)==0?8:0));
    }
    if(address==0xfffffe17)return (activeSlave_?slaveOnChip_:onChip_).requested[0x17]|0xe0;
    if(address>=0xfffffe00&&resetConfiguration(address))return (activeSlave_?slaveOnChip_:onChip_).requested[address-0xfffffe00];
    throw std::runtime_error(at("Native data read service pending",raw));
}
void NativeSession::writeByte(std::uint32_t raw,std::uint8_t value) {
    const auto address=physical(raw);
    if(address>=0x00180000&&address<0x00190000){if((address&1)!=0)backupRam_[(address-0x00180000)/2]=value;return;}
    if(address>=WorkRam::base&&address<WorkRam::base+WorkRam::size){ram_->write8(address,value);return;}
    if(address>=0x05a00000&&address<0x05b00000){soundRam_[address-0x05a00000]=value;return;}
    if(address>=0x00200000&&address<0x00300000){lowRam_[address-0x00200000]=value;return;}
    if(address>=0x00400000&&address<0x00400040){if((address&1)!=0)arcadeIo_.write(address-0x00400000,value);return;}
    if(address>=0x05c00000&&address<0x05cc0000){video_[address-0x05c00000]=value;return;}
    if(address>=0x05e00000&&address<0x05f00000){vdp2Vram_[(address-0x05e00000)&0x7ffff]=value;return;}
    if(address>=0x05f00000&&address<0x05f80000){colorRam_[(address-0x05f00000)&0xfff]=value;return;}
    if(address>=0x05d00000&&address<0x05d0000c) {
        if(address==0x05d00004){vdp1Configuration_[4]=0;return;}
        if(address==0x05d00005) {
            const auto mode=value&3;
            if(mode==3)throw std::runtime_error("Reserved VDP1 plot-trigger mode");
            // Mode 2 arms the vblank-triggered path. It does not submit a draw now;
            // consuming scan/vblank events still requires the native raster service.
            vdp1Configuration_[5]=mode;if(mode==1)submitControlDraw();return;
        }
        vdp1Configuration_[address-0x05d00000]=value;if(address==0x05d00002||address==0x05d00003)framebufferAccessed_=true;return;
    }
    if(address>=0x05f80000&&address<0x05f80200){
        if(address<0x05f80002){
            const auto mode=std::uint16_t(address==0x05f80000?(std::uint16_t(value)<<8)|vdp2Configuration_[1]:(std::uint16_t(vdp2Configuration_[0])<<8)|value);
            raster_.configure(mode,ram_->read32(0x06000324)!=0,now_());
        }
        // TVSTAT/HCNT/VCNT are read-only. Bulk initialization writes are ignored there.
        if((address>=0x05f80004&&address<0x05f80006)||(address>=0x05f80008&&address<0x05f8000c))return;
        if(address==0x05f80006){vdp2Configuration_[6]=value&0x80;return;}
        if(address==0x05f80007)return;
        vdp2Configuration_[address-0x05f80000]=value;return;
    }
    if(address==0x00100075){
        const auto output=std::uint8_t(value&portDirection_[0]&0x7f);
        if((output&4)!=0)throw std::runtime_error("Serial EEPROM transaction pending");
        if((output&3)!=0)throw std::runtime_error("Multiple cartridge bank selection pending");
        portReadback_[0]=output|(value&0x80);return;
    }
    if(address==0x00100077){const auto output=std::uint8_t(value&portDirection_[1]&0x7f);resetSoundPort(output);portReadback_[1]=output|(value&0x80);return;}
    if(address==0x00100079||address==0x0010007b){portDirection_[(address-0x00100079)/2]=value&0x7f;return;}
    if(address==0x0010007d){portMode_=value&3;return;}
    if(address==0x0010007f){
        // Configuration only. No edge is generated by enabling a latch; actual video
        // status/event consumption still stops until the scheduler is implemented.
        externalLatchEnable_=value&3;return;
    }
    if(address==0x0010001f){
        if(value==2)startSlave();
        else if(value==3)platform_.sendSystemCommand(value);
        else if(value==0x19||value==0x1a)resetControlBit_=value&1;
        else if(value==0x10){if(intbackPending_)throw std::runtime_error("SMPC received a command while INTBACK is busy");std::copy_n(smpcInput_.begin(),3,intbackRequest_.begin());intbackPending_=true;intbackContinuation_=false;smpcRequestArmed_=true;intbackStage_=0;intbackDue_=now_()+8000+(smpcInput_[0]!=0?8000:0)+((smpcInput_[1]&8)!=0?700000:0);return;}
        else throw std::runtime_error(at("SMPC command pending",value));
        smpcCompletion_=value;smpcRequestArmed_=false;return;
    }
    if(address==0x00100063){if(value>1)throw std::runtime_error("Unsupported SMPC request flag");smpcRequestArmed_=value!=0;return;}
    if(address>=0x00100000&&address<0x0010000e){
        if((address&1)!=0)smpcInput_[(address-0x00100000)/2]=value;
        if(address==0x00100001&&intbackStage_!=0){
            if((value&0x40)!=0){smpcStatus_&=15;intbackStage_=0;intbackPending_=false;smpcRequestArmed_=false;}
            else if((value&0x80)!=0){intbackContinuation_=true;intbackPending_=true;smpcRequestArmed_=true;intbackDue_=now_()+700000;}
        }
        return;
    }
    if(address==0xfffffe92){(activeSlave_?slaveCode_:code_).originalControl=value;return;}
    // Only the reset-written on-chip configuration has a native contract so far.
    if(address>=0xfffffe00&&resetConfiguration(address)){
        if(address==0xfffffe12||address==0xfffffe13) {
            const unsigned cpu=activeSlave_?1:0;auto base=counter(cpu);const auto shift=(address&1)==0?8:0;
            frcBase_[cpu]=std::uint16_t((base&~(0xffu<<shift))|(std::uint16_t(value)<<shift));frcEpoch_[cpu]=now_();frcLastTicks_[cpu]=0;return;
        }
        if(address==0xfffffe16){const unsigned cpu=activeSlave_?1:0;frcBase_[cpu]=counter(cpu);frcEpoch_[cpu]=now_();frcLastTicks_[cpu]=0;}
        auto& stored=(activeSlave_?slaveOnChip_:onChip_).requested[address-0xfffffe00];
        if(address==0xfffffe11){if((value&1)!=0)throw std::runtime_error("FRC clear-on-compare event mode pending");stored=(value&~0x8e)|(stored&value&0x8e);}
        else if(address==0xfffffe14||address==0xfffffe15) {
            const auto& chip=activeSlave_?slaveOnChip_:onChip_;
            auto& compare=(activeSlave_?slaveTimerCompare_:timerCompare_)[(chip.requested[0x17]&0x10)!=0];
            const auto shift=(address&1)==0?8:0;compare=std::uint16_t((compare&~(0xffu<<shift))|(std::uint16_t(value)<<shift));
        }
        else if(address==0xfffffe16)stored=value&0x83;
        else if(address==0xfffffe17)stored=value&0x13;
        else stored=(address==0xfffffe71||address==0xfffffe72)?value&3:value;
        return;
    }
    throw std::runtime_error(at("Native data write service pending",raw));
}
std::span<std::uint8_t> NativeSession::memoryRange(std::uint32_t address,unsigned size){
    const auto end=std::uint64_t(address)+size;
    if(address>=0x06000000&&end<=0x06100000)return std::span(ram_->bytes).subspan(address-0x06000000,size);
    if(address>=0x00200000&&end<=0x00300000)return std::span(lowRam_).subspan(address-0x00200000,size);
    if(address>=0x05a00000&&end<=0x05b00000)return std::span(soundRam_).subspan(address-0x05a00000,size);
    if(address>=0x05c00000&&end<=0x05cc0000)return std::span(video_).subspan(address-0x05c00000,size);
    if(address>=0x05e00000&&end<=0x05f00000&&(address&0x7ffff)+size<=0x80000)return std::span(vdp2Vram_).subspan(address&0x7ffff,size);
    if(address>=0x05f00000&&end<=0x05f80000&&(address&0xfff)+size<=0x1000)return std::span(colorRam_).subspan(address&0xfff,size);
    return {};
}
std::uint64_t NativeSession::read(std::uint32_t raw,unsigned size) {
    if(size==0||size>8||std::uint64_t(raw)+size>0x100000000ull)throw std::out_of_range("Invalid native data transfer");
    const auto address=physical(raw);
    // If the worker is already at the reset cursor, no wrap remains to
    // acknowledge. Otherwise it must execute its actual cursor reset below;
    // observing the producer once is insufficient because a trailer command
    // rereads it after executing and can miss a rapid refill (an ABA).
    if(renderQueuePublicationPending_&&executingNative_&&activeSlave_&&physical(slaveContext.pc)==0x06062b9a&&address==0x060ffc48&&size==4&&slaveContext.reg(4)==renderQueueResetCursor_)renderQueuePublicationPending_=false;
    if(const auto bytes=memoryRange(address,size);!bytes.empty()){
        if(size==4)return aot::be<4>(bytes,0);if(size==2)return aot::be<2>(bytes,0);if(size==1)return bytes[0];
        std::uint64_t result=0;for(const auto b:bytes)result=(result<<8)|b;return result;
    }
    if(size==2&&(address==0x05f80000||address==0x05f80002||address==0x05f80004||address==0x05f80008||address==0x05f8000a))return readVideoWord(address);
    if(size==4&&address>=0xffffff00&&address<=0xffffff1c&&(address&3)==0)return divider_[activeSlave_?1:0].read(address-0xffffff00);
    if(address>=0x05b00000&&std::uint64_t(address)+size<=0x05b01000){scsp_.poll(now_());std::uint64_t result=0;for(unsigned i=0;i<size;++i){const auto byte=address-0x05b00000+i;result=(result<<8)|std::uint8_t(scsp_.read(byte&~1u)>>((byte&1)==0?8:0));}return result;}
    if(size==2&&address==0xfffffe12)return counter(activeSlave_?1:0);
    if(size==2&&address==0xfffffe18)return frcCapture_[activeSlave_?1:0];
    if(size==4&&address==0xfffffe10){const auto c=counter(activeSlave_?1:0);const auto& chip=activeSlave_?slaveOnChip_:onChip_;return (std::uint32_t(chip.requested[0x10])<<24)|(std::uint32_t(chip.requested[0x11])<<16)|c;}
    if(address>=0x04fffff0&&std::uint64_t(address)+size<=0x05000000&&size<=4){
        if((address&3)+size>4)throw std::runtime_error("Protection read crosses register lanes");
        const auto aligned=address&~3u;std::uint32_t word;
        if(aligned==0x04fffffc&&(protection_.words()[0]&0x10000)!=0){
            const auto first=crypt_->next(),second=crypt_->next();const auto swap=[](std::uint16_t v){return std::uint16_t((v<<8)|(v>>8));};
            word=(std::uint32_t(swap(first))<<16)|swap(second);
        }else word=(std::uint32_t(protection_.read16(aligned,cart_))<<16)|protection_.read16(aligned+2,cart_);
        return (std::uint64_t(word)>>(8*(4-(address&3)-size)))&((1ull<<(8*size))-1);
    }
    if(size==4&&address>=0xffffffe0&&address<=0xffffffec&&(address&3)==0) {
        const auto value=(activeSlave_?slaveBusConfiguration_:busConfiguration_).read((address-0xffffffe0)/4);
        return value|((activeSlave_&&address==0xffffffe0)?0x8000:0);
    }
    if(size==4&&address==0xffffffb0)return (activeSlave_?slaveDma_:platform_.cpuDma).operation();
    if(size==4&&address>=0xffffff80&&address<=0xffffff9c&&(address&3)==0) {
        const auto relative=address-0xffffff80;
        return (activeSlave_?slaveDmaChannels_:dmaChannels_)[relative/16][(relative%16)/4];
    }
    if(address>=0x05fe0000&&std::uint64_t(address)+size<=0x05fe00c0) {
        if(size==4&&address==0x05fe0080)return 1; // Reset-held DSP PC=0; readback is PC+1.
        if(size==4&&address<0x05fe0060){const auto index=(address-0x05fe0000)/4;if(index%8>=3)return 0;}
        std::uint64_t value=0;for(unsigned i=0;i<size;++i){const auto at=address-0x05fe0000+i;value=(value<<8)|std::uint8_t(platform_.scu.words[at/4]>>(24-8*(at%4)));}return value;
    }
    std::uint64_t value=0;for(unsigned i=0;i<size;++i)value=(value<<8)|readByte(raw+i);return value;
}
void NativeSession::write(std::uint32_t raw,unsigned size,std::uint64_t value) {
    if(dataWriteTrace)dataWriteTrace(activeSlave_,activeSlave_?slaveContext.pc:context.pc,physical(raw),size,value);
    if(size==0||size>8||std::uint64_t(raw)+size>0x100000000ull)throw std::out_of_range("Invalid native data transfer");
    const auto address=physical(raw);
    if(renderQueuePublicationPending_&&executingNative_&&activeSlave_&&physical(slaveContext.pc)==0x06062bc4&&address==0x060ffc44&&size==4&&value==renderQueueResetCursor_)renderQueuePublicationPending_=false;
    if(auto bytes=memoryRange(address,size);!bytes.empty()){
        if(size==4)aot::setBe<4>(bytes,0,value);else if(size==2)aot::setBe<2>(bytes,0,value);else if(size==1)bytes[0]=std::uint8_t(value);
        else for(unsigned i=0;i<size;++i)bytes[i]=std::uint8_t(value>>(8*(size-1-i)));return;
    }
    if(address>=0x01000000&&address<0x02000000&&(size==2||size==4)) {
        // MINIT/SINIT pulse the peer's FRT input capture line; data is not a RAM payload.
        (address<0x01800000?slaveOnChip_:onChip_).requested[0x11]|=0x80;
        frcCapture_[address<0x01800000?1:0]=counter(address<0x01800000?1:0);
        if(executingNative_&&!activeSlave_&&physical(context.pc)==0x0605ee10&&address<0x01800000){renderQueueResetCursor_=ram_->read32(0x060ffc48);renderQueuePublicationPending_=true;}
        (activeSlave_?slaveContext:context).yieldRequested=true;return;
    }
    if(size==4&&address>=0xffffff00&&address<=0xffffff1c&&(address&3)==0){divider_[activeSlave_?1:0].write(address-0xffffff00,std::uint32_t(value));return;}
    if(address>=0x05b00000&&std::uint64_t(address)+size<=0x05b01000){
        unsigned consumed=0;while(consumed<size){const auto relative=address-0x05b00000+consumed;
            if((relative&1)==0&&size-consumed>=2){scsp_.write(relative,std::uint16_t(value>>(8*(size-consumed-2))),now_());consumed+=2;}
            else {const unsigned shift=(relative&1)==0?8:0;const auto byte=std::uint8_t(value>>(8*(size-consumed-1)));const auto old=scsp_.read(relative&~1u);scsp_.write(relative&~1u,std::uint16_t((old&~(0xffu<<shift))|(unsigned(byte)<<shift)),now_());++consumed;}
        }return;
    }
    if(size==2&&address==0x057ffffc){platform_.scu.regionBusControl=std::uint16_t(value);return;} // BIOS bus-selection request; no attached device at this address in ST-V map.
    if(size==2&&address==0x05f80000){
        raster_.configure(std::uint16_t(value),ram_->read32(0x06000324)!=0,now_());vdp2Configuration_[0]=std::uint8_t(value>>8);vdp2Configuration_[1]=std::uint8_t(value);return;
    }
    if(size==2&&address==0xfffffe12){const unsigned cpu=activeSlave_?1:0;frcBase_[cpu]=std::uint16_t(value);frcEpoch_[cpu]=now_();frcLastTicks_[cpu]=0;return;}
    if(size<=4&&address>=0x04fffff0&&std::uint64_t(address)+size<=0x05000000){
        if((address&3)+size>4)throw std::runtime_error("Protection write crosses register lanes");
        for(unsigned offset=0;offset<size;){
            const auto at=(address+offset)&~1u;std::uint16_t half;
            if(((address+offset)&1)==0&&size-offset>=2){half=std::uint16_t(value>>(8*(size-2-offset)));offset+=2;}
            else {const unsigned shift=((address+offset)&1)==0?8:0;const auto old=std::uint16_t(protection_.words()[(at-0x04fffff0)/4]>>((at&2)==0?16:0));const auto byte=std::uint8_t(value>>(8*(size-1-offset)));half=std::uint16_t((old&~(0xffu<<shift))|(unsigned(byte)<<shift));++offset;}
            protection_.write16(at,half);
            if(at==0x04fffff8)crypt_->set_addr_low(protection_.requestedAddressLow);
            if(at==0x04fffffa)crypt_->set_addr_high(protection_.requestedAddressHigh);
            if((at&~3u)==0x04fffffc)crypt_->set_subkey(protection_.requestedSubkey);
        }
        return;
    }
    if(size==4&&address>=0xffffffe0&&address<=0xffffffec&&(address&3)==0){(activeSlave_?slaveBusConfiguration_:busConfiguration_).write((address-0xffffffe0)/4,std::uint32_t(value));return;}
    if(size==4&&address==0xffffffb0){
        auto& gate=activeSlave_?slaveDma_:platform_.cpuDma;gate.writeOperation(std::uint32_t(value));
        for(auto& channel:activeSlave_?slaveDmaChannels_:dmaChannels_)transfer_cpu_dma(*this,channel,gate.operation());return;
    }
    if(size==4&&address>=0xffffff80&&address<=0xffffff9c&&(address&3)==0) {
        const auto relative=address-0xffffff80,index=(relative%16)/4;
        auto& channel=(activeSlave_?slaveDmaChannels_:dmaChannels_)[relative/16];
        if(index==3)channel[index]=(std::uint32_t(value)&~2u)|(channel[index]&std::uint32_t(value)&2u);
        else channel[index]=std::uint32_t(value)&(index==2?0xffffffu:0xffffffffu);
        if(index==3)transfer_cpu_dma(*this,channel,(activeSlave_?slaveDma_:platform_.cpuDma).operation());
        return;
    }
    if(size==4&&address>=0x60000000&&address<=0x600003f0&&(address&15)==0) {
        if(value!=0)throw std::runtime_error("Non-reset cache-tag mutation needs a native coherency contract");
        auto& policy=activeSlave_?slaveCode_:code_;policy.resetRequests.push_back({address,0,size});++policy.invalidationGeneration;return;
    }
    if(size==4&&raw>=0x40000000&&raw<0x60000000) {
        // Associative purge is a cache request, not a write to the corresponding RAM.
        // Both native contexts read the same coherent WorkRam and every code entry is
        // source-checked; record the invalidation without corrupting shared data.
        auto& policy=activeSlave_?slaveCode_:code_;++policy.invalidationGeneration;return;
    }
    if(size==4&&address==0x05fe00a0){platform_.interruptMask=std::uint32_t(value);platform_.scu.words[0xa0/4]=std::uint32_t(value);return;}
    if(size==4&&address==0x05fe00a4){platform_.scu.words[0xa4/4]&=std::uint32_t(value);return;}
    if(size==4&&address==0x05fe00a8&&value==1){platform_.scu.words[0xa8/4]=1;return;} // No external A-bus interrupt source connected.
    if(size==4&&address==0x05fe0060&&value==0)return; // No running job to force-stop in synchronous backend.
    if(size==4&&address==0x05fe0080&&value==0){platform_.scu.words[0x80/4]=0;return;}
    if(size==4&&(address==0x05fe0090||address==0x05fe0094)){platform_.scu.words[(address-0x05fe0000)/4]=std::uint32_t(value);return;}
    if(size==4&&address==0x05fe0098&&value==0){platform_.scu.words[0x98/4]=0;return;}
    if(size==4&&(address==0x05fe00b0||address==0x05fe00b4||address==0x05fe00b8)){platform_.scu.words[(address-0x05fe0000)/4]=std::uint32_t(value);return;}
    if(size==4&&address>=0x05fe0000&&address<0x05fe0058&&(address&3)==0){
        const auto index=(address-0x05fe0000)/4,field=index%8,level=index/8;
        if(field>=6)throw std::runtime_error("Reserved SCU DMA register write");
        const auto mask=field<2?0x07ffffffu:field==2?(level==0?0xfffffu:0xfffu):0xffffffffu;
        platform_.scu.words[index]=std::uint32_t(value)&mask;
        if(field==4&&(value&0x101)==0x101&&(platform_.scu.words[level*8+5]&7)==7)runScuDma(level);
        return;
    }
    if(address>=0x05fe0000&&std::uint64_t(address)+size<=0x05fe00c0)throw std::runtime_error(at("SCU operation outside translated reset pending",raw));
    for(unsigned i=0;i<size;++i)writeByte(raw+i,std::uint8_t(value>>(8*(size-1-i))));
}
void NativeSession::verifySource(std::uint32_t address,std::uint32_t expected,unsigned size) {
    if(sourceTrace)sourceTrace(activeSlave_,address);
    // Instruction provenance checks use direct byte views of verified source
    // storage, not the peripheral data-bus dispatch on every native statement.
    // Still compare the current RAM bytes; no cache that conceals code mutation.
    const auto mapped=physical(address);std::span<const std::uint8_t> bytes;
    if(size==0||size>4)throw std::runtime_error("Invalid native source guard width");
    if(mapped>=0x06000000&&std::uint64_t(mapped)+size<=0x06100000)bytes=std::span(ram_->bytes).subspan(mapped-0x06000000,size);
    else if(std::uint64_t(mapped)+size<=0x80000)bytes=bios_.span(mapped,size);
    else throw std::runtime_error(at("Native source guard outside verified code storage",address));
    unsigned actual=0;for(const auto b:bytes)actual=(actual<<8)|b;
    if(actual!=expected)throw std::runtime_error(at("Native block source changed; expected="+std::to_string(expected)+" actual="+std::to_string(actual)+"; retranslation required",address));
}
void NativeSession::validateRange(std::uint32_t raw,std::size_t size,bool writable) {
    const auto address=physical(raw);
    const auto inside=[&](std::uint32_t begin,std::uint32_t end){return address>=begin&&std::uint64_t(address)+size<=end;};
    if(inside(0x06000000,0x06100000)||inside(0x00200000,0x00300000)||inside(0x05c00000,0x05cc0000)||inside(0x05a00000,0x05b00000)||
       inside(0x05e00000,0x05f00000)||inside(0x05f00000,0x05f80000))return;
    if(writable&&inside(0x05f80000,0x05f80200))return;
    if(!writable&&(inside(0,0x80000)||inside(0x02000000,0x04fffff0)))return;
    throw std::runtime_error(at("DMA destination/source device or range pending",raw));
}
}
