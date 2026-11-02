#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <vector>
namespace tecmo98 {
// Optional bounded in-memory diagnostics; no per-iteration disk I/O.
class HostPhases {
 using Clock=std::chrono::steady_clock;
 struct Event {unsigned phase;long long start;double ms;};
 bool enabled_;std::vector<Event> events_;std::array<double,7> maximum_{};
public:
 enum Phase {Messages,AudioOutput,InputFile,Controllers,FrameResult,Gpu,Core};
 explicit HostPhases(bool enabled):enabled_(enabled){if(enabled_)events_.reserve(512);}
 class Scope {
  HostPhases& owner_;Phase phase_;Clock::time_point start_;
 public:
  Scope(HostPhases&o,Phase p):owner_(o),phase_(p){if(o.enabled_)start_=Clock::now();}
  ~Scope(){if(owner_.enabled_){const double ms=std::chrono::duration<double,std::milli>(Clock::now()-start_).count();owner_.maximum_[phase_]=std::max(owner_.maximum_[phase_],ms);if(ms>=8&&owner_.events_.size()<512)owner_.events_.push_back({unsigned(phase_),std::chrono::duration_cast<std::chrono::nanoseconds>(start_.time_since_epoch()).count(),ms});}}
 };
 void save(const char* path)const{if(!enabled_)return;const char* names[]={"messages","audio_output","input_file","controllers","frame_result","gpu","core"};std::ofstream out(path);out<<"kind,phase,host_ns,milliseconds\n";for(auto e:events_)out<<"stall,"<<names[e.phase]<<','<<e.start<<','<<e.ms<<'\n';for(unsigned p=0;p<maximum_.size();++p)out<<"maximum,"<<names[p]<<",0,"<<maximum_[p]<<'\n';}
};
}
