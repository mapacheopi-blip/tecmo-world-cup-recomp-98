#include "native_dxgi_presenter.hpp"
#include "native_pixel_filter.hpp"
#include <iostream>
#include <algorithm>
#include <fstream>
namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
void check(tecmo98::NativeDxgiPresenter& gpu,HWND window,const tecmo98::NativeFrame& raw,unsigned w,unsigned h,int filter,bool stretch){
 SetWindowPos(window,nullptr,0,0,w,h,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
 require(gpu.draw(raw,stretch,filter),"GPU draw failed");const auto actual=gpu.readback();tecmo98::NativeFrame image;
 if(filter==2)tecmo98::scale2x(raw,image);else image=raw;
 const unsigned dw=stretch?w:unsigned(std::min(std::uint64_t(w),std::uint64_t(h)*4/3)),dh=stretch?h:dw*3/4,left=(w-dw)/2,top=(h-dh)/2;
 require(actual.width==w&&actual.height==h,"GPU readback dimensions");
 for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){std::uint32_t expected=0;
  if(x>=left&&x<left+dw&&y>=top&&y<top+dh){const auto sx=std::min(image.width-1,unsigned((double(x-left)+.5)*image.width/dw)),sy=std::min(image.height-1,unsigned((double(y-top)+.5)*image.height/dh));expected=image.pixels[sy*image.width+sx];}
  require((actual.pixels[y*w+x]&0xffffff)==(expected&0xffffff),"GPU point / Pixelart pixel differs from CPU reference");
 }
}
}
int main(int argc,char** argv){HWND window=nullptr;try{
 WNDCLASSW cls{};cls.lpfnWndProc=DefWindowProcW;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"TecmoDxgiVerification";RegisterClassW(&cls);window=CreateWindowExW(0,cls.lpszClassName,L"GPU verification",WS_POPUP,0,0,64,48,nullptr,nullptr,cls.hInstance,nullptr);require(window!=nullptr,"Create test window");
 unsigned cases=0;std::uint32_t seed=1998;
 {
  tecmo98::NativeDxgiPresenter gpu(window,true);
  for(unsigned w=1;w<=12;++w)for(unsigned h=1;h<=8;++h){tecmo98::NativeFrame raw{w,h,std::vector<std::uint32_t>(w*h)};for(auto& p:raw.pixels){seed=seed*1664525+1013904223;p=(seed%5)*0x224466u;}
   check(gpu,window,raw,w*2,h*2,0,true);check(gpu,window,raw,w*2,h*2,2,true);cases+=2;
  }
  tecmo98::NativeFrame raw{13,9,std::vector<std::uint32_t>(13*9)};for(auto& p:raw.pixels){seed=seed*1664525+1013904223;p=seed;}
  for(unsigned n=0;n<100;++n){check(gpu,window,raw,31+n%19,24+n%13,0,false);check(gpu,window,raw,31+n%19,24+n%13,2,false);cases+=2;}
  tecmo98::NativeFrame solid{4,3,std::vector<std::uint32_t>(12,0x7384ac)};SetWindowPos(window,nullptr,0,0,64,48,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);require(gpu.draw(solid,false,1),"Linear filter draw");for(auto p:gpu.readback().pixels)require((p&0xffffff)==0x7384ac,"Linear filter changes solid color");
  bool rejected=false;try{gpu.draw({4,3,{1}},false,0);}catch(const std::exception&){rejected=true;}require(rejected,"Invalid frame accepted");
 }
 // A real hardware device must also render correctly; game uses this path.
 {tecmo98::NativeDxgiPresenter hardware(window);tecmo98::NativeFrame raw{4,3,{0x0000ff,0x00ff00,0xff0000,0xffffff,0xff0000,0x0000ff,0x00ff00,0,0xffffff,0xff0000,0x0000ff,0x00ff00}};check(hardware,window,raw,8,6,0,true);check(hardware,window,raw,8,6,2,true);cases+=2;std::cout<<"Adapter: "<<hardware.adapter()<<'\n';
  if(argc==2){std::ifstream in(argv[1],std::ios::binary);BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};in.read(reinterpret_cast<char*>(&header),sizeof(header));in.read(reinterpret_cast<char*>(&info),sizeof(info));require(header.bfType==0x4d42&&info.biWidth>0&&info.biWidth<=4096&&info.biHeight<0&&info.biHeight>=-4096&&info.biBitCount==32&&info.biCompression==BI_RGB,"Invalid native bitmap");raw={unsigned(info.biWidth),unsigned(-info.biHeight),std::vector<std::uint32_t>(std::size_t(info.biWidth)*-info.biHeight)};in.seekg(header.bfOffBits);in.read(reinterpret_cast<char*>(raw.pixels.data()),raw.pixels.size()*4);require(bool(in),"Truncated native bitmap");
   for(auto size:std::array<std::pair<unsigned,unsigned>,4>{{{960,720},{1280,720},{1920,1080},{3840,2160}}})for(int filter:{0,2}){check(hardware,window,raw,size.first,size.second,filter,false);++cases;}
  }
 }
 DestroyWindow(window);window=nullptr;
 std::cout<<"{\"passed\":true,\"gpu_readback_cases\":"<<cases<<",\"raw_and_pixelart_byte_exact_rgb\":true,\"warp_and_hardware\":true,\"letterbox_and_resize\":true,\"linear_solid_color\":true,\"invalid_input_rejected\":true}\n";return 0;
}catch(const std::exception& e){if(window)DestroyWindow(window);std::cerr<<e.what()<<'\n';return 1;}}
