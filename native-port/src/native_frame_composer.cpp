#include "native_frame_composer.hpp"
#include "native_pixel_filter.hpp"
namespace tecmo98 {
NativeFrameComposer::~NativeFrameComposer(){ {std::lock_guard lock(mutex_);closing_=true;pending_.reset();}changed_.notify_all();worker_.join();}
std::uint64_t NativeFrameComposer::submit(const NativeVideoView& v,std::uint64_t time,std::uint64_t sourceFrame){
    Snapshot s;s.submitted=std::chrono::steady_clock::now();s.sequence=++nextSequence_;s.time=time;s.sourceFrame=sourceFrame;unsigned i=0;
    for(auto bytes:{v.display,v.draw,v.vram,v.color,v.vdp1,v.vdp2}){s.buffers[i].assign(bytes.begin(),bytes.end());++i;}
    s.copyMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-s.submitted).count();
    const auto sequence=s.sequence;{std::lock_guard lock(mutex_);if(pending_)++replaced_;pending_=std::move(s);}changed_.notify_one();return sequence;
}
std::optional<NativeFrameComposer::Result> NativeFrameComposer::take(){std::lock_guard lock(mutex_);auto result=std::move(ready_);ready_.reset();return result;}
void NativeFrameComposer::recycleFrames(NativeFrame frame,NativeFrame presentation){std::lock_guard lock(mutex_);if(!frame.pixels.empty())recycledFrame_=std::move(frame);if(!presentation.pixels.empty())recycledPresentation_=std::move(presentation);}
std::optional<NativeFrameComposer::Result> NativeFrameComposer::waitFor(std::uint64_t sequence,std::chrono::milliseconds timeout){
    std::unique_lock lock(mutex_);if(!changed_.wait_for(lock,timeout,[&]{return ready_&&ready_->sequence>=sequence;}))return {};auto result=std::move(ready_);ready_.reset();return result;
}
void NativeFrameComposer::work(){for(;;){Snapshot s;Result r;{
    std::unique_lock lock(mutex_);changed_.wait(lock,[&]{return closing_||pending_.has_value();});if(closing_)return;s=std::move(*pending_);pending_.reset();r.frame=std::move(recycledFrame_);r.presentation=std::move(recycledPresentation_);}
    r.sequence=s.sequence;r.time=s.time;r.sourceFrame=s.sourceFrame;r.submitted=s.submitted;r.submitMs=s.copyMs;const auto start=std::chrono::steady_clock::now();r.queueMs=std::chrono::duration<double,std::milli>(start-s.submitted).count();const auto& b=s.buffers;const int filter=presentationFilter_.load();
    if(b[5].size()>=34){r.videoMode=std::uint16_t((b[5][0]<<8)|b[5][1]);r.layers=std::uint16_t((b[5][32]<<8)|b[5][33]);}
    try{compose_native_frame_into({b[0],b[1],b[2],b[3],b[4],b[5]},r.frame);const auto composed=std::chrono::steady_clock::now();r.composeMs=std::chrono::duration<double,std::milli>(composed-start).count();if(filter==2)scale2x(r.frame,r.presentation);else{r.presentation.width=r.presentation.height=0;r.presentation.pixels.clear();}r.filterMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-composed).count();}catch(const std::exception& e){r.error=e.what();}
    workNs_+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();++rendered_;
    {std::lock_guard lock(mutex_);if(ready_){if(recycledFrame_.pixels.empty())recycledFrame_=std::move(ready_->frame);if(recycledPresentation_.pixels.empty())recycledPresentation_=std::move(ready_->presentation);}ready_=std::move(r);}changed_.notify_all();
}}
}
