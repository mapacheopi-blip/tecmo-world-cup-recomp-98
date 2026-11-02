#include "native_presentation_clock.hpp"
#include <iostream>
#include <stdexcept>
using namespace std::chrono;
int main(){try{
 const steady_clock::time_point epoch{};tecmo98::NativePresentationClock clock(epoch);
 if(clock.due(epoch)||clock.due(epoch+milliseconds(16))||!clock.due(epoch+milliseconds(17)))throw std::runtime_error("First deadline");
 const auto next=clock.next();if(next!=epoch+nanoseconds(33333334)||clock.due(epoch+milliseconds(18)))throw std::runtime_error("Late call shifted future deadline");
 if(!clock.due(epoch+milliseconds(120))||clock.skipped()!=5||clock.due(epoch+milliseconds(120))||clock.next()!=epoch+nanoseconds(133333336))throw std::runtime_error("Stall queued a burst or shifted cadence");
 tecmo98::NativePresentationClock jitter(epoch);unsigned frames=0;for(unsigned us=0;us<10000000;us+=137)frames+=jitter.due(epoch+microseconds(us));
 if(frames!=599||jitter.skipped()!=0)throw std::runtime_error("Small scheduling jitter accumulated");
 std::cout<<"{\"passed\":true,\"absolute_deadlines\":true,\"no_catchup_burst\":true,\"jitter_does_not_accumulate\":true,\"frames_in_ten_seconds\":"<<frames<<"}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
