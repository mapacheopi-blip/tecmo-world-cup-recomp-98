#pragma once
#include "native_video.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
namespace tecmo98 {
// Presentation owns immutable copies; it never reads a live NativeSession.
// At most one waiting snapshot and one completed frame bound latency/memory.
class NativeFrameComposer {
    struct Snapshot {std::array<std::vector<std::uint8_t>,6> buffers;std::uint64_t sequence,time,sourceFrame;std::chrono::steady_clock::time_point submitted;double copyMs=0;};
public:
    struct Result {NativeFrame frame,presentation;std::string error;std::uint64_t sequence=0,time=0,sourceFrame=0;std::uint16_t videoMode=0,layers=0;std::chrono::steady_clock::time_point submitted;double queueMs=0,composeMs=0,filterMs=0,submitMs=0;};
private:
    std::mutex mutex_;std::condition_variable changed_;std::optional<Snapshot> pending_;std::optional<Result> ready_;bool closing_=false;
    std::atomic<int> presentationFilter_;
    NativeFrame recycledFrame_,recycledPresentation_;
    std::thread worker_;std::uint64_t nextSequence_=0;
    std::atomic<std::uint64_t> rendered_{0},workNs_{0},replaced_{0};
    void work();
public:
    explicit NativeFrameComposer(int presentationFilter=0):presentationFilter_(presentationFilter),worker_([this]{work();}){}
    ~NativeFrameComposer();
    std::uint64_t submit(const NativeVideoView&,std::uint64_t gameTime,std::uint64_t sourceFrame=0);
    void setPresentationFilter(int filter){if(filter<0||filter>2)throw std::invalid_argument("Invalid presentation filter");presentationFilter_.store(filter);}
    std::optional<Result> take();
    void recycleFrames(NativeFrame,NativeFrame);
    std::optional<Result> waitFor(std::uint64_t sequence,std::chrono::milliseconds timeout);
    auto rendered()const{return rendered_.load();}auto replaced()const{return replaced_.load();}
    double averageMilliseconds()const{const auto n=rendered();return n?double(workNs_.load())/double(n)/1e6:0;}
};
}
