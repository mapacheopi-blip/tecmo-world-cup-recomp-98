// Native PCM signal processing. Envelope timing and the slot mixing/loop
// conventions are adapted from MAME 0.289 scsp.cpp (BSD-3-Clause).
// Copyright ElSemi and R. Belmont. See third-party/mame-scsp/NOTICE.md.
#include "native_scsp.hpp"
#include "native_scsp_tables.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <cstdlib>
#include <iostream>
#include <fstream>

namespace tecmo98 {
void NativeScsp::keyExecute(unsigned slot,std::uint16_t value){
    for(unsigned i=0;i<32;++i){auto& voice=voices_[i];const auto key=i==slot?value:words_[i*16];
        if((key&0x800)!=0&&voice.phase==3){const auto base=i*16;
            if(ram_.size()!=0x100000)throw std::runtime_error("SCSP PCM has no attached sample RAM");
            if((key&0x180)!=0)throw std::runtime_error("SCSP internal waveform synthesis pending at slot "+std::to_string(i));
            if(words_[base+7]!=0)throw std::runtime_error("SCSP phase modulation pending at slot "+std::to_string(i));
            if((words_[base+9]&0xff)!=0)throw std::runtime_error("SCSP LFO modulation pending at slot "+std::to_string(i));
            if(words_[base+3]<=words_[base+2]&&((key>>5)&3)!=0)throw std::runtime_error("SCSP inverted loop interval");
            voice={true,false,0,0,4096,383};
            if(std::getenv("TECMO98_SCSP_TRACE")){static unsigned captures=0;if(captures<2){std::ofstream out("native-keyon-"+std::to_string(++captures)+".bin",std::ios::binary);out.write(reinterpret_cast<const char*>(ram_.data()),ram_.size());}std::cerr<<"SCSP keyon time="<<double(rendered_)/44100<<" slot="<<i<<" words="<<std::hex;for(unsigned j=0;j<12;++j)std::cerr<<(j==0?key:words_[base+j])<<",";std::cerr<<std::dec<<"\n";}
        }else if((key&0x800)==0){voice.phase=3;words_[i*16]&=~0x800u;}
    }
}
void NativeScsp::renderUntil(std::uint64_t now){
    if(!audioClockStarted_){audioClockStarted_=true;epoch_=now;}
    const auto elapsed=now-epoch_,target=(elapsed/1000000000)*44100+((elapsed%1000000000)*44100)/1000000000;
    if(target<rendered_)throw std::runtime_error("SCSP monotonic audio clock moved backwards");
    if(target-rendered_>441000)throw std::runtime_error("SCSP audio backlog exceeds ten seconds");
    const auto rate=[](const double* times,int base,unsigned value,bool attack){const auto index=std::clamp(base+int(value*2),0,63);if(index<2)return 0.0;if(attack&&times[index]==0)return 1024.0;return 1023.0*1000/(44100*times[index]);};
    constexpr double send[8]={0,0.0158489319246,0.0316227766017,0.063095734448,0.125892541179,0.251188643151,0.501187233627,1};
    for(;rendered_<target;++rendered_){double left=0,right=0;
        for(unsigned i=0;i<32;++i){auto& voice=voices_[i];if(!voice.active)continue;const auto w=std::span(words_).subspan(i*16,16);const auto key=w[0];
            if((key&0x180)!=0||w[7]!=0||(w[9]&0xff)!=0)throw std::runtime_error("SCSP active waveform/modulation pending");
            const unsigned start=((key&15)<<16)|w[1],loopStart=w[2],loopEnd=w[3],loop=(key>>5)&3;const bool pcm8=(key&16)!=0,direct=(w[6]&0x100)!=0;
            // ST-V maps the full 20-bit sample address space (1 MiB).
            const auto sampleAt=[&](std::int64_t position){const unsigned a=(start+unsigned(position>>12)*(pcm8?1:2))&0xfffff;
                return pcm8?int(std::int8_t(ram_[a]))*256:int(std::int16_t((unsigned(ram_[a])<<8)|ram_[(a+1)&0xfffff]));};
            const auto fraction=unsigned(voice.position)&4095;int sample=int((std::int64_t(sampleAt(voice.position))*(4096-fraction)+std::int64_t(sampleAt(voice.next))*fraction)>>12);
            if((key&0x200)!=0)sample^=0x7fff;if((key&0x400)!=0)sample=std::int16_t(sample^0x8000);
            const int octave=int((w[8]>>11)&15)^8,oct=octave-8;const int shift=oct+2;std::int64_t step=(w[8]&1023)+1024;step=shift>=0?step<<shift:step>>-shift;
            voice.position+=voice.backwards?-step:step;voice.next=voice.position+4096;
            if(voice.position>=std::int64_t(loopStart)*4096&&!voice.backwards&&(w[5]&0x4000)!=0&&voice.phase==0)voice.phase=1;
            for(auto* position:{&voice.position,&voice.next}){
                if(loop==0){if(*position>=std::int64_t(std::max(loopStart,loopEnd))*4096){voice.active=false;w[0]&=~0x800u;}}
                else if(loop==1){if(*position>=std::int64_t(loopEnd)*4096)*position=std::int64_t(loopStart)*4096+(*position-std::int64_t(loopEnd)*4096);}
                else if(loop==2){if(!voice.backwards&&*position>=std::int64_t(loopStart)*4096){*position=std::int64_t(loopEnd+loopStart)*4096-*position;voice.backwards=true;}else if(voice.backwards&&*position<std::int64_t(loopStart)*4096)*position=std::int64_t(loopEnd-loopStart)*4096+*position;}
                else if(*position>=std::int64_t(loopEnd)*4096){*position=std::int64_t(loopEnd)*8192-*position;voice.backwards=true;}else if(voice.backwards&&*position<std::int64_t(loopStart)*4096){*position=std::int64_t(loopStart)*8192-*position;voice.backwards=false;}
            }
            if(!direct){const int krs=(w[5]>>10)&15,base=krs==15?0:oct+2*krs+((w[8]>>9)&1);const bool attack=voice.phase==0;
                if(voice.phase==0){voice.envelope=std::min(1023.0,voice.envelope+rate(ARTimes,base,w[4]&31,true));if(voice.envelope==1023&&(w[5]&0x4000)==0)voice.phase=1;}
                else {const unsigned field=voice.phase==1?(w[4]>>6)&31:voice.phase==2?(w[4]>>11)&31:w[5]&31;voice.envelope=std::max(0.0,voice.envelope-((voice.phase==2&&field==0)?0:rate(DRTimes,base,field,false)));
                    if(voice.phase==1&&(unsigned(voice.envelope)>>5)<=31-((w[5]>>5)&31))voice.phase=2;if(voice.phase==3&&voice.envelope==0){voice.active=false;w[0]&=~0x800u;}}
                const double gain=attack?((w[4]&32)!=0?1:voice.envelope/1024):std::pow(10.0,3*(voice.envelope-1023)/32/20);sample=int(sample*gain);
            }
            const unsigned tl=direct?0:w[6]&255,pan=(w[11]>>8)&31,level=(w[11]>>13)&7;double db=0;constexpr double tlDb[8]={.4,.8,1.5,3,6,12,24,48};for(unsigned b=0;b<8;++b)if((tl&(1u<<b))!=0)db-=tlDb[b];
            const double gain=std::pow(10.0,db/20)*send[level];double pDb=0;
            // Pan attenuation uses 3, 6, 12, 24 dB; the opposite channel stays unity.
            pDb= -3*int(pan&15);const double pGain=(pan&15)==15?0:std::pow(10.0,pDb/20);
            left+=sample*gain*(pan<16?pGain:1);right+=sample*gain*(pan<16?1:pGain);
            // DSP remains zero only while no program is running (nonzero MPRO
            // starts are rejected by write()). PCM direct output is independent.
        }
        const auto volume=double(words_[0x400/2]&15)/15;const auto l=std::int16_t(std::clamp(int(left*volume),-32768,32767)),r=std::int16_t(std::clamp(int(right*volume),-32768,32767));
        if(l!=0||r!=0)++nonzeroSamples_;if(output_.size()>=44100*4){output_.pop_front();output_.pop_front();}output_.push_back(l);output_.push_back(r);
    }
}
}
