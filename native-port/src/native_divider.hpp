#pragma once
#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace tecmo98 {
class NativeDivider {
    std::uint32_t divisor_=0,high_=0,low_=0,control_=0,vector_=0;
    bool remainderValid_=true;
    void divide(std::int64_t numerator){
        const std::int64_t denominator=std::bit_cast<std::int32_t>(divisor_);
        const bool exceptional=denominator==0||(numerator==std::numeric_limits<std::int64_t>::min()&&denominator==-1);
        const auto quotient=exceptional?0:numerator/denominator;
        if(exceptional||quotient<std::numeric_limits<std::int32_t>::min()||quotient>std::numeric_limits<std::int32_t>::max()){
            // Hitachi ADE-602-085C 10.3.3: OVFIE=0 saturates the quotient
            // according to its sign. DVDNTH retains the partial six-cycle
            // result, not a saturated remainder. Keep that unreadable until
            // the non-restoring divider's intermediate state is implemented.
            control_|=1;remainderValid_=false;
            if((control_&2)!=0)throw std::runtime_error("DIVU overflow interrupt/intermediate quotient pending");
            low_=(numerator<0)!=(denominator<0)?0x80000000u:0x7fffffffu;return;
        }
        low_=std::uint32_t(quotient);high_=std::uint32_t(numerator%denominator);remainderValid_=true;
    }
public:
    std::uint32_t read(unsigned offset)const {
        switch(offset){case 0:return divisor_;case 4:case 20:case 28:return low_;case 8:return control_;case 12:return vector_;case 16:case 24:if(!remainderValid_)throw std::runtime_error("DIVU partial overflow remainder pending");return high_;default:throw std::out_of_range("Unknown DIVU register");}
    }
    void write(unsigned offset,std::uint32_t value){
        switch(offset){
            case 0:divisor_=value;return;case 4:low_=value;divide(std::bit_cast<std::int32_t>(value));return;
            case 8:control_=value&3;return;case 12:vector_=value&0x7f;return;case 16:high_=value;remainderValid_=true;return;
            case 20:low_=value;divide(std::bit_cast<std::int64_t>((std::uint64_t(high_)<<32)|low_));return;
            default:throw std::out_of_range("Read-only/unknown DIVU register write");
        }
    }
};
}
