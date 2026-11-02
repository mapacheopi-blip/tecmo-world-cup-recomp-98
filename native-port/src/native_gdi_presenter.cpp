#include "native_gdi_presenter.hpp"
#include "native_pixel_filter.hpp"
#include <algorithm>
#include <limits>
namespace tecmo98 {
void NativeGdiPresenter::release(){
 if(memory_&&original_)SelectObject(memory_,original_);
 if(bitmap_)DeleteObject(bitmap_);
 if(memory_)DeleteDC(memory_);
 memory_=nullptr;bitmap_=nullptr;original_=nullptr;width_=height_=0;
}
bool NativeGdiPresenter::resize(HDC target,int width,int height){
 if(memory_&&width_==width&&height_==height)return true;
 const auto dc=CreateCompatibleDC(target);if(!dc)return false;
 BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),width,-height,1,32,BI_RGB,0,0,0,0,0};
 void* pixels=nullptr;const auto bitmap=CreateDIBSection(target,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
 if(!bitmap){DeleteDC(dc);return false;}
 const auto original=SelectObject(dc,bitmap);
 if(!original||original==HGDI_ERROR){DeleteObject(bitmap);DeleteDC(dc);return false;}
 release();memory_=dc;bitmap_=bitmap;original_=original;width_=width;height_=height;++allocations_;return true;
}
bool NativeGdiPresenter::paint(HDC target,int width,int height,const NativeFrame& frame){
 if(width<=0||height<=0)return true; // Minimized window; retain the last buffer.
 if(!target||frame.width>unsigned(std::numeric_limits<int>::max())||frame.height>unsigned(std::numeric_limits<int>::max())||frame.pixels.size()!=std::size_t(frame.width)*frame.height)return false;
 if(!resize(target,width,height))return false;
 const RECT area{0,0,width,height};if(!FillRect(memory_,&area,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH))))return false;
 if(frame.width&&frame.height){
  if(edge2x_)scale2x(frame,filtered_);
  const auto& image=edge2x_?filtered_:frame;
  BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),LONG(image.width),-LONG(image.height),1,32,BI_RGB,0,0,0,0,0};
  const int drawWidth=stretch_?width:int(std::min(std::int64_t(width),std::int64_t(height)*4/3)),drawHeight=stretch_?height:drawWidth*3/4;
  if(drawWidth&&drawHeight){SetStretchBltMode(memory_,smooth_?HALFTONE:COLORONCOLOR);if(smooth_)SetBrushOrgEx(memory_,0,0,nullptr);
   const auto lines=StretchDIBits(memory_,(width-drawWidth)/2,(height-drawHeight)/2,drawWidth,drawHeight,0,0,int(image.width),int(image.height),image.pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);
   if(lines==GDI_ERROR||lines==0)return false;
  }
 }
 // All clearing and scaling are complete before a single window copy.
 if(!BitBlt(target,0,0,width,height,memory_,0,0,SRCCOPY))return false;
 ++presentations_;return true;
}
}
