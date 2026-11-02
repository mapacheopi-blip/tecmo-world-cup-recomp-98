#pragma once
#include "recompiled.hpp"
#include "image_loader.hpp"
#include "boot.hpp"
#include "native_dma.hpp"
#include "native_video.hpp"
#include "native_io.hpp"
#include "native_divider.hpp"
#include "native_crypt5881.hpp"
#include "recompiled_sound.hpp"
#include "native_scsp.hpp"
#include <functional>
#include <memory>
#include <ctime>

namespace tecmo98 {
// Native data/services adapter for the statically compiled path. Unknown hardware
// behavior stops explicitly; allocating a register image is not a device engine.
class NativeSession final:public aot::DataBus,public NativeDmaMemory {
    class SoundBus final:public aot::DataBus {
        NativeSession& owner_;
    public:
        explicit SoundBus(NativeSession& owner):owner_(owner){}
        std::uint64_t read(std::uint32_t,unsigned)override;
        void write(std::uint32_t,unsigned,std::uint64_t)override;
        void verifySource(std::uint32_t,std::uint32_t,unsigned)override;
    } soundBus_{*this};
    FirmwareData bios_;
    CartridgeImage cart_;
    std::unique_ptr<WorkRam> ram_;
    std::vector<std::uint8_t> lowRam_,video_,vdp2Vram_,colorRam_;
    std::vector<std::uint8_t> soundRam_=std::vector<std::uint8_t>(0x100000);
    NativeScsp scsp_;
    bool soundMainIrq_=false;
    bool soundHeld_=true,activeSound_=false;
    std::array<std::uint8_t,0x8000> backupRam_{};
    std::vector<std::uint8_t> displayFramebuffer_=std::vector<std::uint8_t>(0x40000);
    bool framebufferAccessed_=false,eraseNextFrame_=false;
    std::array<std::uint8_t,0x20> vdp1Configuration_{};
    std::array<std::uint8_t,0x200> vdp2Configuration_{};
    OnChipResetConfiguration onChip_;
    OnChipResetConfiguration slaveOnChip_;
    NativeCodePolicy code_;
    NativeCodePolicy slaveCode_;
    NativePlatform platform_;
    NativeBusConfiguration busConfiguration_;
    NativeBusConfiguration slaveBusConfiguration_;
    CpuDmaDispatchGate slaveDma_;
    std::array<std::array<std::uint32_t,4>,2> dmaChannels_{},slaveDmaChannels_{};
    std::array<std::uint16_t,2> timerCompare_{},slaveTimerCompare_{};
    ProtectionRegisters protection_;
    std::unique_ptr<NativeCrypt5881> crypt_;
    NativeArcadeIo arcadeIo_;
    std::array<NativeDivider,2> divider_;
    bool smpcRequestArmed_=false;
    // jp1 boot writes observed at ROM 0x3130/0x3156 in the isolated reference.
    // This belongs to the partial resident handoff profile, not hardware reset.
    std::array<std::uint8_t,2> portDirection_{0x3f,0x18};
    std::array<std::uint8_t,2> portReadback_{0,0x10};
    std::uint8_t portMode_=3,externalLatchEnable_=3;
    std::uint8_t smpcCompletion_=3,resetControlBit_=0;
    std::uint8_t smpcStatus_=0;
    std::array<std::uint8_t,7> smpcInput_{};
    std::array<std::uint8_t,32> smpcOutput_{};
    std::array<std::uint8_t,3> intbackRequest_{};
    unsigned intbackStage_=0;
    bool intbackPending_=false,intbackContinuation_=false;
    std::uint64_t intbackDue_=0;
    bool activeSlave_=false;
    unsigned schedulerPhase_=0;
    std::uint64_t schedulerQuantumLeft_=64;
    bool renderQueuePublicationPending_=false;
    std::uint32_t renderQueueResetCursor_=0;
    bool executingNative_=false;
    std::uint64_t zeroVectorAngleFixes_=0;
    bool fixZeroVectorAngle(aot::Context&);
    std::function<std::uint64_t()> now_;
    std::function<std::uint64_t()> hostNow_;
    std::uint64_t excludedHostWork_=0,hostPauseStarted_=0;
    unsigned hostPauseDepth_=0;
    std::array<std::uint64_t,3> hostWorkTime_{};
    std::time_t rtcEpoch_=0;
    std::uint64_t rtcGameEpoch_=0;
    class HostWorkScope {
        NativeSession& owner_;unsigned category_;
    public:
        explicit HostWorkScope(NativeSession& owner,unsigned category=0):owner_(owner),category_(category){if(owner_.hostPauseDepth_++==0)owner_.hostPauseStarted_=owner_.hostNow_();}
        ~HostWorkScope(){if(--owner_.hostPauseDepth_==0){const auto elapsed=owner_.hostNow_()-owner_.hostPauseStarted_;owner_.excludedHostWork_+=elapsed;owner_.hostWorkTime_[category_]+=elapsed;}}
    };
    class HostCostScope {
        NativeSession& owner_;unsigned category_;std::uint64_t start_;
    public:
        HostCostScope(NativeSession& owner,unsigned category):owner_(owner),category_(category),start_(owner.hostNow_()){}
        ~HostCostScope(){owner_.hostWorkTime_[category_]+=owner_.hostNow_()-start_;}
    };
    NativeRasterClock raster_;
    std::uint64_t rasterGeneration_=0,lastRasterTicks_=0,drawLists_=0,videoFrames_=0;
    std::uint16_t latchedH_=0,latchedV_=0;
    bool externalLatchFlag_=false;
    std::uint32_t pendingScu_=0;
    CommandListState gpuState_;
    NativeVdp1Job gpuJob_;
    std::uint64_t gpuEpoch_=0,gpuTicks_=0;
    std::uint16_t gpuStatus_=0,gpuLast_=0;
    std::array<std::uint16_t,2> frcBase_{},frcCapture_{};
    std::array<std::uint64_t,2> frcEpoch_{},frcLastTicks_{};
    void startSlave();
    void deliverCaptureInterrupt();
    RasterSample pollVideo();
    std::uint16_t readVideoWord(std::uint32_t address);
    void postScu(unsigned source);
    void runScuDma(unsigned level,bool notify=true);
    void triggerScuDma(unsigned event);
    void submitControlDraw();
    void pollGpu();
    void pollSmpc();
    void resetSoundPort(std::uint8_t output);
    void runSound(std::uint64_t instructions);
    void deliverSoundInterrupt();
    void frameBoundary();
    void eraseFramebuffer(std::span<std::uint8_t> target);
    std::uint16_t counter(unsigned cpu);
    std::uint64_t readByte(std::uint32_t address);
    void writeByte(std::uint32_t address,std::uint8_t value);
    std::span<std::uint8_t> memoryRange(std::uint32_t address,unsigned size);
public:
    std::function<void(bool,std::uint32_t)> sourceTrace;
    std::function<void(bool,std::uint32_t,std::uint32_t,unsigned,std::uint64_t)> dataWriteTrace;
    std::function<void(std::uint32_t,std::uint8_t)> ioReadTrace;
    std::function<void(bool,int,unsigned,std::uint32_t,std::uint32_t)> interruptTrace;
    std::function<void(std::uint32_t)> soundSourceTrace;
    // Borrowed view, valid only during this synchronous callback. Consumers
    // must copy it; never retain pointers to mutable session memory.
    std::function<void(const NativeVideoView&,std::uint64_t,std::uint64_t)> videoFrameReady;
    aot::Context context;
    aot::Context slaveContext;
    aot_sound::Context soundContext;
    NativeSession(const std::filesystem::path& eprom,const std::filesystem::path& bios,
                  const std::filesystem::path& cart,const std::filesystem::path& residentProfile,
                  std::function<std::uint64_t()> monotonicNanoseconds={});
    std::uint64_t read(std::uint32_t address,unsigned size) override;
    void write(std::uint32_t address,unsigned size,std::uint64_t value) override;
    void verifySource(std::uint32_t address,std::uint32_t expected,unsigned bytes) override;
    bool runNativeFunction(aot::Context& c)override{return fixZeroVectorAngle(c);}
    void validateRange(std::uint32_t address,std::size_t size,bool writable)override;
    void runCooperative(std::uint64_t maximumInstructions);
    bool activeSlave() const{return activeSlave_;}
    bool activeSound()const{return activeSound_;}
    std::uint64_t completedDrawLists()const{return drawLists_;}
    std::uint64_t videoFrames()const{return videoFrames_;}
    NativeVideoView videoView()const{return {displayFramebuffer_,std::span(video_).subspan(0x80000,0x40000),vdp2Vram_,colorRam_,vdp1Configuration_,vdp2Configuration_};}
    NativeFrame composeFrame();
    const auto& hostWorkNanoseconds()const{return hostWorkTime_;}
    std::uint64_t gameNanoseconds()const{return now_();}
    std::vector<std::int16_t> takeAudio(){return scsp_.takeAudio();}
    std::uint64_t nonzeroAudioSamples()const{return scsp_.nonzeroSamples();}
    std::uint64_t zeroVectorAngleFixes()const{return zeroVectorAngleFixes_;}
    bool renderPublicationPending()const{return renderQueuePublicationPending_;}
    void setInputContacts(unsigned port,std::uint8_t pressed){arcadeIo_.contacts(port,pressed);}
};
}
