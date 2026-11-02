#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <span>
#include <vector>
#include <deque>

namespace tecmo98 {
// SCSP register/timer backend. Active synthesis/DSP requests remain explicit
// boundaries until the corresponding native signal-processing path exists.
class NativeScsp {
    std::array<std::uint16_t,0x800> words_{};
    struct Timer{std::uint64_t epoch=0;unsigned base=0,prescaler=1;bool armed=false,expired=false;};
    std::array<Timer,3> timers_{};
    struct Voice {bool active=false,backwards=false;unsigned phase=3;std::int64_t position=0,next=4096;double envelope=383;};
    std::array<Voice,32> voices_{};
    std::span<const std::uint8_t> ram_;
    std::deque<std::int16_t> output_;
    std::uint64_t rendered_=0,epoch_=0;
    std::uint64_t nonzeroSamples_=0;
    bool audioClockStarted_=false;
    void renderUntil(std::uint64_t now);
    void keyExecute(unsigned slot,std::uint16_t value);
public:
    void attachRam(std::span<const std::uint8_t> ram){ram_=ram;}
    std::vector<std::int16_t> takeAudio(){std::vector<std::int16_t> result(output_.begin(),output_.end());output_.clear();return result;}
    std::uint64_t nonzeroSamples()const{return nonzeroSamples_;}
    void poll(std::uint64_t now){
        renderUntil(now);
        for(unsigned i=0;i<3;++i){auto& t=timers_[i];if(!t.armed)continue;const auto elapsed=now-t.epoch;
            const auto ticks=((elapsed/1000000000)*44100+((elapsed%1000000000)*44100)/1000000000)/t.prescaler;
            const auto counter=t.base+ticks;
            words_[(0x418+i*2)/2]=std::uint16_t((words_[(0x418+i*2)/2]&0xff00)|(counter>=255?255:counter));
            if(counter>=255){t.armed=false;t.expired=true;words_[0x420/2]|=std::uint16_t(0x40u<<i);if(i==0)words_[0x42c/2]|=0x40;}
        }
    }
    unsigned irqLevel()const {
        const auto pending=words_[0x420/2]&words_[0x41e/2];unsigned bit;
        if((pending&0x40)!=0)bit=6;else if((pending&0x180)!=0)bit=7;else if((pending&8)!=0)bit=3;else return 0;
        return ((words_[0x424/2]>>bit)&1)|(((words_[0x426/2]>>bit)&1)<<1)|(((words_[0x428/2]>>bit)&1)<<2);
    }
    bool mainIrq()const{return (words_[0x42c/2]&words_[0x42a/2])!=0;}
    std::uint16_t read(unsigned address)const {
        if(address>=0x1000||(address&1)!=0)throw std::out_of_range("SCSP register read alignment/range");
        if(address>=0x430&&address<0x600)return 0;
        if(address>=0x7c0&&address<0x800)address-=0x40;
        // CA reports bits 12..15 of the sample index; MSLC is write-only.
        if(address==0x408){const auto slot=(words_[address/2]>>11)&31;return std::uint16_t(((unsigned(voices_[slot].position>>24)&15)<<7)|(voices_[slot].phase<<5)|((31-(unsigned(voices_[slot].envelope)>>5))&31));}
        return words_[address/2];
    }
    void write(unsigned address,std::uint16_t value,std::uint64_t now){
        if(address>=0x1000||(address&1)!=0)throw std::out_of_range("SCSP register write alignment/range");poll(now);
        if(address>=0x430&&address<0x600)return;
        if(address>=0xc00)return; // DSP result storage is read-only from the host interface.
        if(address>=0x7c0&&address<0x800)address-=0x40;
        if(address<0x400&&(address&0x1f)==0&&(value&0x1000)!=0){
            keyExecute(address/32,value);
            value&=~0x1000u;
        }
        if(address==0x416&&(value&0x1000)!=0)throw std::runtime_error("SCSP internal DMA engine pending");
        if(address==0xbf0){for(unsigned at=0x800;at<0xc00;at+=2)if((at==address?value:words_[at/2])!=0)throw std::runtime_error("SCSP DSP signal processing pending");}
        if(address==0x420){words_[address/2]=(words_[address/2]&~0x20u)|(value&0x20);return;}
        if(address==0x42c){words_[address/2]|=value&0x20;return;}
        if(address==0x422){
            words_[0x420/2]&=~value;
            for(unsigned i=0;i<3;++i)if(timers_[i].expired)words_[0x420/2]|=std::uint16_t(0x40u<<i);
        }
        if(address==0x42e)words_[0x42c/2]&=~value;
        words_[address/2]=value;
        if(address>=0x418&&address<=0x41c){auto& t=timers_[(address-0x418)/2];t={now,unsigned(value&255),1u<<((value>>8)&7),(value&255)!=255,false};}
    }
};
}
