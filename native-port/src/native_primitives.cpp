#include "native_video.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace tecmo98 {
void draw_vdp1_primitive(std::span<const std::uint8_t> vram,unsigned position,const CommandListState& state,
                         std::span<std::uint8_t> framebuffer,bool eightBit,unsigned framebufferControl) {
    if(vram.size()!=0x80000||framebuffer.size()!=0x40000||position>0x7ffe0)throw std::invalid_argument("Invalid native primitive buffers/command range");
    const auto word=[&](unsigned at){at&=0x7ffff;return unsigned((vram[at]<<8)|vram[(at+1)&0x7ffff]);};
    const auto control=word(position),kind=control&15;
    if(kind==0){draw_vdp1_normal_sprite(vram,position,state,framebuffer,eightBit,framebufferControl);return;}
    if(kind!=1&&kind!=2&&kind!=4&&kind!=5&&kind!=6)throw std::runtime_error("VDP1 primitive pending: type="+std::to_string(kind)+" offset="+std::to_string(position));
    const auto mode=word(position+4),color=word(position+6),gouraud=word(position+28)*8;
    if((mode&7)==5)throw std::runtime_error("Prohibited VDP1 color calculation mode");
    const bool textured=kind==1||kind==2;const auto size=word(position+10),textureWidth=((size>>8)&63)*8,textureHeight=size&255,format=(mode>>3)&7,source=word(position+8)*8;
    if(textured&&(textureWidth==0||textureHeight==0))return;if(textured&&format>5)throw std::runtime_error("Reserved VDP1 texture format");
    struct Vertex{int x,y;std::array<int,3> shade;int u=0,v=0;};std::array<Vertex,4> vertices;
    const auto coordinate=[](unsigned v){return int(v&0x3ff)-int(v&0x400);};
    for(unsigned i=0;i<4;++i){const auto shade=word(gouraud+i*2);vertices[i]={coordinate(word(position+12+i*4))+state.localX,coordinate(word(position+14+i*4))+state.localY,{int(shade&31)-16,int((shade>>5)&31)-16,int((shade>>10)&31)-16}};}
    vertices[1].u=vertices[2].u=int(textureWidth)-1;vertices[2].v=vertices[3].v=int(textureHeight)-1;
    if(kind==1){
        const auto zoom=(control>>8)&15;int left=vertices[0].x,top=vertices[0].y,right=vertices[2].x,bottom=vertices[2].y;
        if(zoom!=0){const int width=coordinate(word(position+16)),height=coordinate(word(position+18));
            if((zoom&3)==2)left-=width/2;else if((zoom&3)==3)left-=width;
            if((zoom>>2)==2)top-=height/2;else if((zoom>>2)==3)top-=height;
            right=left+width;bottom=top+height;
        }
        vertices[0].x=vertices[3].x=left;vertices[1].x=vertices[2].x=right;
        vertices[0].y=vertices[1].y=top;vertices[2].y=vertices[3].y=bottom;
    }
    const int stride=eightBit?1024:512;
    const bool field=(framebufferControl&8)!=0;const int clipRight=std::min(stride-1,int(state.clipRight)),clipBottom=std::min(field?511:255,int(state.clipBottom));
    const auto [minX,maxX]=std::minmax_element(vertices.begin(),vertices.end(),[](const Vertex& a,const Vertex& b){return a.x<b.x;});
    const auto [minY,maxY]=std::minmax_element(vertices.begin(),vertices.end(),[](const Vertex& a,const Vertex& b){return a.y<b.y;});
    if(maxX->x<0||minX->x>clipRight||maxY->y<0||minY->y>clipBottom)return;
    const auto plot=[&](int x,int y,const std::array<int,3>& shade,unsigned sampled){
        const bool interlaced=(framebufferControl&8)!=0;
        if(x<0||y<0||x>=stride||y>=(interlaced?512:256)||x>int(state.clipRight)||y>int(state.clipBottom))return;
        if(interlaced&&unsigned(y&1)!=((framebufferControl>>2)&1))return;
        const bool inside=x>=state.userLeft&&y>=state.userTop&&x<=state.userRight&&y<=state.userBottom;
        if((mode&0x400)!=0&&(inside==((mode&0x200)!=0)))return;
        if((mode&0x100)!=0&&((x^y)&1)==0)return;
        if(!textured&&color==0&&(mode&0x40)==0)return;
        const auto at=(unsigned(interlaced?y/2:y)*stride+unsigned(x))*(eightBit?1:2);
        const unsigned old=eightBit?framebuffer[at]:(framebuffer[at]<<8)|framebuffer[at+1];
        unsigned value=sampled;
        if((mode&4)!=0){value=sampled&0x8000;for(unsigned c=0;c<3;++c)value|=unsigned(std::clamp(int((sampled>>(5*c))&31)+shade[c],0,31))<<(5*c);}
        const auto half=[](unsigned v){return ((v&0x7bde)>>1)|0x8000;};
        if((mode&3)==1){if((old&0x8000)==0)return;value=half(old);}
        else if((mode&3)==2)value=half(value);
        else if((mode&3)==3&&(old&0x8000)!=0){unsigned result=0x8000;for(unsigned c=0;c<3;++c)result|=((((value>>(5*c))&31)+((old>>(5*c))&31))/2)<<(5*c);value=result;}
        if((mode&0x8000)!=0)value|=0x8000;
        if(eightBit)framebuffer[at]=std::uint8_t(value);else{framebuffer[at]=std::uint8_t(value>>8);framebuffer[at+1]=std::uint8_t(value);}
    };
    const auto interpolate=[&](const Vertex& a,const Vertex& b,int n,int d){Vertex v=a;if(d!=0){v.x+=int(std::int64_t(b.x-a.x)*n/d);v.y+=int(std::int64_t(b.y-a.y)*n/d);if(textured){v.u+=int(std::int64_t(b.u-a.u)*n/d);v.v+=int(std::int64_t(b.v-a.v)*n/d);}if((mode&4)!=0)for(unsigned c=0;c<3;++c)v.shade[c]+=int(std::int64_t(b.shade[c]-a.shade[c])*n/d);}return v;};
    const auto line=[&](const Vertex& a,const Vertex& b){
        if((a.x<0&&b.x<0)||(a.x>clipRight&&b.x>clipRight)||(a.y<0&&b.y<0)||(a.y>clipBottom&&b.y>clipBottom))return;
        const int steps=std::max(std::abs(b.x-a.x),std::abs(b.y-a.y));int first=0,last=steps;
        if(steps!=0&&(!textured||(mode&0x80)!=0)){
            // Clip the parameter range conservatively. A one-dot margin retains
            // the original integer truncation at both ends; plot applies the
            // exact inclusive hardware/system rectangle afterwards.
            double lo=0,hi=steps;
            const auto bound=[&](int start,int delta,int limit){if(delta==0)return;double l=double(-1-start)*steps/delta,h=double(limit+1-start)*steps/delta;if(l>h)std::swap(l,h);lo=std::max(lo,l);hi=std::min(hi,h);};
            bound(a.x,b.x-a.x,clipRight);bound(a.y,b.y-a.y,clipBottom);
            if(lo>hi)return;first=std::max(0,int(std::floor(lo)));last=std::min(steps,int(std::ceil(hi)));
        }
        // Exact integer DDA: a+trunc((b-a)*i/steps), with divisions once
        // per line rather than for every pixel. This preserves the old raster
        // positions, texture coordinates and Gouraud rounding byte for byte.
        struct Stepper{int value,whole,remainder,error,sign,divisor;
            Stepper(int a,int b,int i,int d):value(a),whole(0),remainder(0),error(0),sign(b<a?-1:1),divisor(d){if(d!=0){const auto delta=std::abs(b-a);value+=int(std::int64_t(b-a)*i/d);whole=(delta/d)*sign;remainder=delta%d;error=int(std::int64_t(delta)*i%d);}}
            void next(){value+=whole;error+=remainder;if(divisor!=0&&error>=divisor){error-=divisor;value+=sign;}}
        };
        Stepper xx(a.x,b.x,first,steps),yy(a.y,b.y,first,steps),uu(a.u,b.u,first,steps),vv(a.v,b.v,first,steps);
        std::array<Stepper,3> shades={Stepper(a.shade[0],b.shade[0],first,steps),Stepper(a.shade[1],b.shade[1],first,steps),Stepper(a.shade[2],b.shade[2],first,steps)};
        const auto advance=[&]{xx.next();yy.next();if(textured){uu.next();vv.next();}if((mode&4)!=0)for(auto& shade:shades)shade.next();};
        unsigned endcodes=0;for(int i=first;i<=last;++i,advance()){Vertex v=a;v.x=xx.value;v.y=yy.value;if(textured){v.u=uu.value;v.v=vv.value;}if((mode&4)!=0)for(unsigned c=0;c<3;++c)v.shade[c]=shades[c].value;unsigned sampled=color;
        if((mode&0x80)!=0&&(v.x<0||v.x>clipRight||v.y<0||v.y>clipBottom||(field&&unsigned(v.y&1)!=((framebufferControl>>2)&1))))continue;
        if(textured){const auto u=(control&0x10)!=0?textureWidth-1-unsigned(v.u):unsigned(v.u),y=(control&0x20)!=0?textureHeight-1-unsigned(v.v):unsigned(v.v),index=y*textureWidth+u;unsigned raw,endcode;
            if(format<=1){const auto packed=vram[(source+index/2)&0x7ffff];raw=(index&1)!=0?packed&15:packed>>4;sampled=format==0?(color&0xfff0)|raw:word(color*8+raw*2);endcode=15;}
            else if(format<5){const unsigned mask=format==2?63:format==3?127:255;raw=vram[(source+index)&0x7ffff];sampled=(color&~mask)|(raw&mask);endcode=255;}
            else {raw=word(source+index*2);sampled=raw;endcode=0x7fff;}
            if((mode&0x80)==0&&raw==endcode){if(++endcodes==2)break;continue;}
            if((mode&0x40)==0&&(format==5?(raw&0x8000)==0:raw==0))continue;
        }
        plot(v.x,v.y,v.shade,sampled);
    }};
    if(kind==6){line(vertices[0],vertices[1]);return;}
    if(kind==5){for(unsigned i=0;i<4;++i)line(vertices[i],vertices[(i+1)%4]);return;}
    if(kind==4||kind==1||kind==2){
        // Convex polygons and textured quads must cover their interior. Parallel integer DDA
        // diagonals can leave holes even in a two-pixel-wide rotated quad.
        // Intersect each framebuffer row with the original edges, then span it.
        bool positive=false,negative=false;
        for(unsigned i=0;i<4;++i){const auto& a=vertices[i];const auto& b=vertices[(i+1)%4];const auto& c=vertices[(i+2)%4];const auto cross=std::int64_t(b.x-a.x)*(c.y-b.y)-std::int64_t(b.y-a.y)*(c.x-b.x);positive|=cross>0;negative|=cross<0;}
        if(!(positive&&negative)&&(positive||negative)){
            struct Hit{double x,u,v;std::array<double,3> shade;};
            for(int y=std::max(0,minY->y);y<=std::min(clipBottom,maxY->y);++y){
                if(field&&unsigned(y&1)!=((framebufferControl>>2)&1))continue;
                std::array<Hit,8> hits;unsigned count=0;
                for(unsigned i=0;i<4;++i){const auto& a=vertices[i];const auto& b=vertices[(i+1)%4];
                    if(y<std::min(a.y,b.y)||y>std::max(a.y,b.y))continue;
                    const auto add=[&](double t){Hit h;h.x=a.x+(b.x-a.x)*t;h.u=a.u+(b.u-a.u)*t;h.v=a.v+(b.v-a.v)*t;for(unsigned c=0;c<3;++c)h.shade[c]=a.shade[c]+(b.shade[c]-a.shade[c])*t;hits[count++]=h;};
                    if(a.y==b.y){add(0);add(1);}else add(double(y-a.y)/double(b.y-a.y));
                }
                if(!count)continue;const auto bounds=std::minmax_element(hits.begin(),hits.begin()+count,[](const Hit& a,const Hit& b){return a.x<b.x;});
                const auto& left=*bounds.first;const auto& right=*bounds.second;
                const int x0=std::max(0,int(std::ceil(left.x-1e-9))),x1=std::min(clipRight,int(std::floor(right.x+1e-9)));if(x0>x1)continue;
                const auto endpoint=[&](int x){Vertex v=vertices[0];v.x=x;v.y=y;const double t=right.x==left.x?0:(x-left.x)/(right.x-left.x);v.u=int(left.u+(right.u-left.u)*t);v.v=int(left.v+(right.v-left.v)*t);for(unsigned c=0;c<3;++c)v.shade[c]=int(left.shade[c]+(right.shade[c]-left.shade[c])*t);return v;};
                line(endpoint(x0),endpoint(x1));
            }return;
        }
    }
    // Native software scan conversion across the two opposing polygon edges.
    // Exact VDP1 edge rounding/overdraw still requires image comparison with hardware.
    const int rows=std::max({std::abs(vertices[3].x-vertices[0].x),std::abs(vertices[3].y-vertices[0].y),std::abs(vertices[2].x-vertices[1].x),std::abs(vertices[2].y-vertices[1].y)});
    for(int row=0;row<=rows;++row)line(interpolate(vertices[0],vertices[3],row,rows),interpolate(vertices[1],vertices[2],row,rows));
}
}
