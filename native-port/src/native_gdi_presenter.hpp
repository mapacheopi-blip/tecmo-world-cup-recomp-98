#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "native_video.hpp"
namespace tecmo98 {
// Complete letterbox + scaled image off screen, then touch the window once.
// Persistent resources are replaced only after a successful resize allocation.
class NativeGdiPresenter {
 HDC memory_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ original_=nullptr;
 int width_=0,height_=0;
 bool stretch_=false,smooth_=false,edge2x_=false;
 NativeFrame filtered_;
 std::uint64_t presentations_=0,allocations_=0;
 bool resize(HDC,int,int);
 void release();
public:
 NativeGdiPresenter()=default;
 ~NativeGdiPresenter(){release();}
 NativeGdiPresenter(const NativeGdiPresenter&)=delete;
 NativeGdiPresenter& operator=(const NativeGdiPresenter&)=delete;
 void configure(bool stretch,int filter){stretch_=stretch;smooth_=filter==1;edge2x_=filter==2;}
 bool paint(HDC target,int width,int height,const NativeFrame&);
 auto presentations()const{return presentations_;}
 auto allocations()const{return allocations_;}
};
}
