#include "native_gdi_presenter.hpp"
#include "native_pixel_filter.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>
struct Surface {
 HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ old=nullptr;std::uint32_t* pixels=nullptr;
 Surface(){dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),64,-64,1,32,BI_RGB,0,0,0,0,0};void* p=nullptr;bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&p,nullptr,0);if(!dc||!bitmap)throw std::runtime_error("Cannot allocate test surface");pixels=static_cast<std::uint32_t*>(p);old=SelectObject(dc,bitmap);}
 ~Surface(){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}
};
static void require(bool x,const char* msg){if(!x)throw std::runtime_error(msg);}
int main(){try{
 Surface target;tecmo98::NativeFrame frame{4,3,{0xff0000,0xff0000,0x00ff00,0x00ff00,0xff0000,0xff0000,0x00ff00,0x00ff00,0x0000ff,0x0000ff,0xffffff,0xffffff}};
 tecmo98::NativeFrame corner{3,3,{0,1,0,1,2,0,0,0,0}},scaled;const auto original=corner.pixels;tecmo98::scale2x(corner,scaled);
 require(scaled.width==6&&scaled.height==6&&corner.pixels==original,"Pixel scaling mutated source or dimensions");
 require(scaled.pixels[14]==1&&scaled.pixels[15]==2&&scaled.pixels[20]==2&&scaled.pixels[21]==0,"Pixel corner reconstruction differs");
 tecmo98::NativeFrame uniform{2,2,{7,7,7,7}};tecmo98::scale2x(uniform,scaled);require(std::all_of(scaled.pixels.begin(),scaled.pixels.end(),[](auto p){return p==7;}),"Uniform pixels changed");
 const auto resources=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
 {tecmo98::NativeGdiPresenter presenter;
  require(presenter.paint(target.dc,4,3,frame),"Initial paint failed");GdiFlush();
  for(unsigned y=0;y<3;++y)for(unsigned x=0;x<4;++x)require((target.pixels[y*64+x]&0xffffff)==frame.pixels[y*4+x],"DIB color/order/orientation changed");
  for(unsigned i=0;i<50;++i)require(presenter.paint(target.dc,4,3,frame),"Persistent paint failed");
  require(presenter.allocations()==1,"Back buffer allocated for every frame");
  require(presenter.paint(target.dc,8,3,frame),"Wide resize failed");GdiFlush();
  for(unsigned y=0;y<3;++y)for(unsigned x=0;x<8;++x)require((target.pixels[y*64+x]&0xffffff)==(x>=2&&x<6?frame.pixels[y*4+x-2]:0),"Wide resize letterbox leaked old image");
  require(presenter.paint(target.dc,4,7,frame),"Tall resize failed");GdiFlush();
  for(unsigned y=0;y<7;++y)for(unsigned x=0;x<4;++x)require((target.pixels[y*64+x]&0xffffff)==(y>=2&&y<5?frame.pixels[(y-2)*4+x]:0),"Tall resize letterbox leaked old image");
  require(presenter.paint(target.dc,8,6,frame),"Scaling failed");GdiFlush();
  for(unsigned y=0;y<6;++y)for(unsigned x=0;x<8;++x)require((target.pixels[y*64+x]&0xffffff)==frame.pixels[(y/2)*4+x/2],"Nearest pixel scaling changed");
  presenter.configure(true,false);require(presenter.paint(target.dc,8,3,frame),"Stretch mode failed");GdiFlush();
  for(unsigned y=0;y<3;++y)for(unsigned x=0;x<8;++x)require((target.pixels[y*64+x]&0xffffff)==frame.pixels[y*4+x/2],"Stretch mode did not fill output");
  presenter.configure(false,true);require(presenter.paint(target.dc,8,6,frame),"Smooth mode failed");GdiFlush();
  presenter.configure(true,2);require(presenter.paint(target.dc,8,6,frame),"Pixel 2x paint failed");GdiFlush();
  tecmo98::scale2x(frame,scaled);for(unsigned y=0;y<6;++y)for(unsigned x=0;x<8;++x)require((target.pixels[y*64+x]&0xffffff)==scaled.pixels[y*8+x],"Pixel 2x output changed");
  presenter.configure(false,false);
  const auto count=presenter.presentations();require(presenter.paint(target.dc,0,0,frame)&&presenter.presentations()==count,"Minimized paint published an empty image");
  auto invalid=frame;invalid.pixels.pop_back();require(!presenter.paint(target.dc,4,3,invalid),"Invalid image silently accepted");
  for(unsigned i=0;i<500;++i)require(presenter.paint(target.dc,16+int(i%16),16+int(i%13),frame),"Resize stress failed");
  GdiFlush();require(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<=resources+2,"GDI resources leaked across resizes");
 }
 GdiFlush();require(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==resources,"Presenter leaked resources after destruction");
 std::cout<<"{\"passed\":true,\"pixel_order_and_orientation\":true,\"letterbox_resize\":true,\"scaling\":true,\"buffer_reuse\":true,\"resize_stress\":500,\"gdi_leaks\":0,\"minimized_preserves_buffer\":true,\"invalid_frame_rejected\":true}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
