#pragma once
#include "game_routines.hpp"

namespace tecmo98 {
class NativeDmaMemory {
public:
    virtual ~NativeDmaMemory()=default;
    virtual std::uint64_t read(std::uint32_t address,unsigned size)=0;
    virtual void write(std::uint32_t address,unsigned size,std::uint64_t value)=0;
    virtual void validateRange(std::uint32_t address,std::size_t size,bool writable)=0;
};
// Native ordered data transfer for the automatic, non-interrupting CPU-DMA subset
// used during SDK setup. Timing, 16-byte bursts and device/FIFO destinations stop.
bool transfer_cpu_dma_high_ram(WorkRam& ram,std::array<std::uint32_t,4>& channel,std::uint32_t operation);
bool transfer_cpu_dma(NativeDmaMemory& memory,std::array<std::uint32_t,4>& channel,std::uint32_t operation);
}
