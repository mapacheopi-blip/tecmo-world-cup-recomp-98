#pragma once
#include "native_video.hpp"
#include <stdexcept>
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif
namespace tecmo98 {
// Edge-aware 2x pixel scaling. The original frame remains immutable.
inline void scale2x(const NativeFrame& source,NativeFrame& target){
 if(!source.width||!source.height||source.width>4096||source.height>4096||source.pixels.size()!=std::size_t(source.width)*source.height)
  throw std::runtime_error("Invalid source for pixel scaling");
 target.width=source.width*2;target.height=source.height*2;target.pixels.resize(std::size_t(target.width)*target.height);
 for(unsigned y=0;y<source.height;++y){
  const auto* row=source.pixels.data()+std::size_t(y)*source.width;
  const auto* above=source.pixels.data()+std::size_t(y?y-1:y)*source.width;
  const auto* below=source.pixels.data()+std::size_t(y+1<source.height?y+1:y)*source.width;
  auto* top=target.pixels.data()+std::size_t(y*2)*target.width;auto* bottom=top+target.width;
  auto scalar=[&](unsigned x){const auto e=row[x],b=above[x],h=below[x],d=row[x?x-1:x],f=row[x+1<source.width?x+1:x];const bool edge=b!=h&&d!=f;
   top[x*2]=edge&&d==b?d:e;top[x*2+1]=edge&&b==f?f:e;bottom[x*2]=edge&&d==h?d:e;bottom[x*2+1]=edge&&h==f?f:e;};
  unsigned x=0;scalar(x++);
#if defined(__SSE2__) || defined(_M_X64)
  // SSE2 is part of the Windows x64 baseline. Four source pixels produce
  // eight pixels on each output row, with exactly the same comparisons.
  for(;x+4<source.width;x+=4){
   const auto e=_mm_loadu_si128(reinterpret_cast<const __m128i*>(row+x));
   const auto b=_mm_loadu_si128(reinterpret_cast<const __m128i*>(above+x)),h=_mm_loadu_si128(reinterpret_cast<const __m128i*>(below+x));
   const auto d=_mm_loadu_si128(reinterpret_cast<const __m128i*>(row+x-1)),f=_mm_loadu_si128(reinterpret_cast<const __m128i*>(row+x+1));
   const auto excluded=_mm_or_si128(_mm_cmpeq_epi32(b,h),_mm_cmpeq_epi32(d,f));
   auto choose=[&](__m128i equal,__m128i neighbor){const auto m=_mm_andnot_si128(excluded,equal);return _mm_or_si128(_mm_and_si128(m,neighbor),_mm_andnot_si128(m,e));};
   const auto a0=choose(_mm_cmpeq_epi32(d,b),d),a1=choose(_mm_cmpeq_epi32(b,f),f),a2=choose(_mm_cmpeq_epi32(d,h),d),a3=choose(_mm_cmpeq_epi32(h,f),f);
   _mm_storeu_si128(reinterpret_cast<__m128i*>(top+x*2),_mm_unpacklo_epi32(a0,a1));_mm_storeu_si128(reinterpret_cast<__m128i*>(top+x*2+4),_mm_unpackhi_epi32(a0,a1));
   _mm_storeu_si128(reinterpret_cast<__m128i*>(bottom+x*2),_mm_unpacklo_epi32(a2,a3));_mm_storeu_si128(reinterpret_cast<__m128i*>(bottom+x*2+4),_mm_unpackhi_epi32(a2,a3));
  }
#endif
  for(;x<source.width;++x)scalar(x);
 }
}
}
