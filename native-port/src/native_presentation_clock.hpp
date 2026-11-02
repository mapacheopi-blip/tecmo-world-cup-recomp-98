#pragma once
#include <chrono>
#include <cstdint>
namespace tecmo98 {
// Absolute host deadlines: late work never shifts all following frames.
// Missed slots are skipped, never queued or replayed as a burst.
class NativePresentationClock {
 using Clock=std::chrono::steady_clock;
 const Clock::duration period_=std::chrono::nanoseconds(16666667);
 Clock::time_point next_;
 std::uint64_t skipped_=0;
public:
 explicit NativePresentationClock(Clock::time_point epoch):next_(epoch+period_){}
 bool due(Clock::time_point now){if(now<next_)return false;const auto missed=(now-next_)/period_;skipped_+=missed;next_+=period_*(missed+1);return true;}
 auto next()const{return next_;}auto skipped()const{return skipped_;}
};
}
