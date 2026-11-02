#pragma once
#include "native_video.hpp"
#include <stdexcept>
namespace tecmo98 {
// Edge-aware 2x pixel scaling. The original frame remains immutable.
inline void scale2x_reference(const NativeFrame& source,NativeFrame& target){
 if(!source.width||!source.height||source.width>4096||source.height>4096||source.pixels.size()!=std::size_t(source.width)*source.height)
  throw std::runtime_error("Invalid source for pixel scaling");
 target.width=source.width*2;target.height=source.height*2;target.pixels.resize(std::size_t(target.width)*target.height);
 for(unsigned y=0;y<source.height;++y)for(unsigned x=0;x<source.width;++x){
  const auto e=source.pixels[y*source.width+x];
  const auto b=source.pixels[(y?y-1:y)*source.width+x],h=source.pixels[(y+1<source.height?y+1:y)*source.width+x];
  const auto d=source.pixels[y*source.width+(x?x-1:x)],f=source.pixels[y*source.width+(x+1<source.width?x+1:x)];
  const auto p=std::size_t(y*2)*target.width+x*2;
  target.pixels[p]=b!=h&&d!=f&&d==b?d:e;
  target.pixels[p+1]=b!=h&&d!=f&&b==f?f:e;
  target.pixels[p+target.width]=b!=h&&d!=f&&d==h?d:e;
  target.pixels[p+target.width+1]=b!=h&&d!=f&&h==f?f:e;
 }
}
}

