#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

namespace tecmo98 {
// Sega 315-5649 parallel input adapter. Logical host input is active-high;
// the game observes active-low JAMMA contacts, including simultaneous buttons.
class NativeArcadeIo {
    std::array<std::uint8_t,7> pressed_{};
    std::uint8_t output_=0,direction_=0,mode_=0,counterSelect_=0;
public:
    void contacts(unsigned port,std::uint8_t pressed){pressed_.at(port)=pressed;}
    std::uint8_t read(unsigned address)const {
        switch(address&0x1f){
            case 1:return std::uint8_t(~pressed_[0]);case 3:return std::uint8_t(~pressed_[1]);
            case 5:return std::uint8_t(~pressed_[2]);case 7:return output_;
            case 9:return std::uint8_t(~pressed_[4]);case 11:return std::uint8_t(~pressed_[5]);
            case 13:if((mode_&0x80)!=0)throw std::runtime_error("Analog arcade counter input pending");return std::uint8_t(~pressed_[6]);
            case 0x1b:return 0; // No serial satellite transaction pending.
            case 0x1d:return mode_;
            default:return 0xff; // Undriven lanes/registers in the reference board map.
        }
    }
    void write(unsigned address,std::uint8_t value){
        switch(address&0x1f){
            case 7:output_=value;return;case 0x11:direction_=value;return;
            case 13:counterSelect_=value;return;case 0x1d:mode_=value;return;
            case 0x13:case 0x15:throw std::runtime_error("Arcade serial satellite output pending");
            default:return; // Unconnected parallel outputs do not change the contacts.
        }
    }
};
}
