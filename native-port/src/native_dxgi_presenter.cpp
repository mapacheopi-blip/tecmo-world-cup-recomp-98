#include "native_dxgi_presenter.hpp"
#include <d3dcompiler.h>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <memory>
#include <type_traits>

namespace tecmo98 {
namespace {
void check(HRESULT hr,const char* name){if(FAILED(hr)){std::ostringstream out;out<<name<<" failed: 0x"<<std::hex<<unsigned(hr);throw std::runtime_error(out.str());}}
const char shader[]=R"(
Texture2D<float4> image:register(t0);
SamplerState linearSampler:register(s0);
cbuffer Settings:register(b0){uint imageWidth;uint imageHeight;uint filter;uint pad;uint viewportX;uint viewportY;uint viewportWidth;uint viewportHeight;};
struct Vertex{float4 position:SV_Position;float2 uv:TEXCOORD;};
Vertex vs(uint id:SV_VertexID){Vertex v;v.uv=float2((id<<1)&2,id&2);v.position=float4(v.uv*float2(2,-2)+float2(-1,1),0,1);return v;}
float4 get(int2 p){return image.Load(int3(clamp(p,int2(0,0),int2(imageWidth-1,imageHeight-1)),0));}
float4 ps(Vertex v):SV_Target{
 if(filter==1){float4 c=image.Sample(linearSampler,v.uv);return float4(c.rgb,1);}
 uint2 outputPixel=uint2(v.position.xy)-uint2(viewportX,viewportY);
 uint2 virtualSize=uint2(imageWidth,imageHeight)*(filter==2?2:1);
 int2 p=int2(((outputPixel*2+1)*virtualSize)/(uint2(viewportWidth,viewportHeight)*2));
 if(filter!=2)return float4(get(p).rgb,1);
 int2 sub=p&1;p>>=1;
 float4 e=get(p),b=get(p+int2(0,-1)),h=get(p+int2(0,1)),d=get(p+int2(-1,0)),f=get(p+int2(1,0));
 if(any(b!=h)&&any(d!=f)){
  if(sub.y==0){if(sub.x==0&&all(d==b))e=d;else if(sub.x==1&&all(b==f))e=f;}
  else {if(sub.x==0&&all(d==h))e=d;else if(sub.x==1&&all(h==f))e=f;}
 }
 return float4(e.rgb,1);
})";
}
NativeDxgiPresenter::NativeDxgiPresenter(HWND window,bool warp):window_(window){
 const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};D3D_FEATURE_LEVEL level;
 check(D3D11CreateDevice(nullptr,warp?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,3,D3D11_SDK_VERSION,&device_,&level,&context_),"D3D11CreateDevice");
 Ptr<IDXGIDevice> dxgi;check(device_.As(&dxgi),"DXGI device");Ptr<IDXGIAdapter> adapter;check(dxgi->GetAdapter(&adapter),"DXGI adapter");DXGI_ADAPTER_DESC description{};check(adapter->GetDesc(&description),"Adapter description");
 char name[512]{};WideCharToMultiByte(CP_UTF8,0,description.Description,-1,name,sizeof(name),nullptr,nullptr);adapter_=name;
 Ptr<IDXGIFactory2> factory;check(adapter->GetParent(IID_PPV_ARGS(&factory)),"DXGI factory");
 DXGI_SWAP_CHAIN_DESC1 desc{};RECT bounds{};GetClientRect(window,&bounds);desc.Width=std::max(1L,bounds.right);desc.Height=std::max(1L,bounds.bottom);desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;desc.Scaling=DXGI_SCALING_STRETCH;desc.AlphaMode=DXGI_ALPHA_MODE_IGNORE;desc.Flags=flags_;
 Ptr<IDXGISwapChain1> chain;check(factory->CreateSwapChainForHwnd(device_.Get(),window,&desc,nullptr,nullptr,&chain),"Create flip swap chain");check(chain.As(&chain_),"Swap chain latency interface");check(factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER),"Window association");
 check(chain_->SetMaximumFrameLatency(1),"Maximum frame latency");
 // Get the owned wait handle last, after all throwing initialization below.
 const std::unique_ptr<std::remove_pointer_t<HMODULE>,decltype(&FreeLibrary)> compiler(LoadLibraryExW(L"d3dcompiler_47.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32),FreeLibrary);if(!compiler)throw std::runtime_error("Windows shader compiler unavailable");
 const auto compile=reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(compiler.get(),"D3DCompile"));
 if(!compile)throw std::runtime_error("Missing D3DCompile");
 Ptr<ID3DBlob> vs,ps,errors;HRESULT hr=compile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"vs","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&errors);
 if(SUCCEEDED(hr))hr=compile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"ps","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&errors);
 if(FAILED(hr))throw std::runtime_error(errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"Shader compilation failed");
 check(device_->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex_),"Vertex shader");check(device_->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel_),"Pixel shader");
 D3D11_BUFFER_DESC cb{};cb.ByteWidth=32;cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;check(device_->CreateBuffer(&cb,nullptr,&constants_),"Shader constants");
 D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.MaxLOD=D3D11_FLOAT32_MAX;check(device_->CreateSamplerState(&sampler,&linear_),"Sampler");
 D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;check(device_->CreateRasterizerState(&raster,&rasterizer_),"Rasterizer");
 resize(desc.Width,desc.Height);latency_=chain_->GetFrameLatencyWaitableObject();if(!latency_)throw std::runtime_error("No DXGI latency handle");
}
NativeDxgiPresenter::~NativeDxgiPresenter(){if(context_){context_->ClearState();context_->Flush();}if(latency_)CloseHandle(latency_);}
void NativeDxgiPresenter::resize(unsigned width,unsigned height){
 if(width==width_&&height==height_&&target_)return;
 context_->OMSetRenderTargets(0,nullptr,nullptr);target_.Reset();
 check(chain_->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,flags_),"Resize swap chain");
 Ptr<ID3D11Texture2D> buffer;check(chain_->GetBuffer(0,IID_PPV_ARGS(&buffer)),"Back buffer");check(device_->CreateRenderTargetView(buffer.Get(),nullptr,&target_),"Backbuffer view");width_=width;height_=height;
}
bool NativeDxgiPresenter::ready(){
 if(occluded_){if(GetTickCount64()-occlusionCheck_<250)return false;occlusionCheck_=GetTickCount64();const auto hr=chain_->Present(0,DXGI_PRESENT_TEST);if(hr==DXGI_STATUS_OCCLUDED)return false;check(hr,"Occlusion test");occluded_=false;}
 if(!token_){const auto state=WaitForSingleObject(latency_,0);if(state==WAIT_FAILED)throw std::runtime_error("DXGI wait handle failed");token_=state==WAIT_OBJECT_0;}
 return token_;
}
bool NativeDxgiPresenter::draw(const NativeFrame& frame,bool stretch,int filter){
 if(!frame.width||!frame.height||frame.width>4096||frame.height>4096||frame.pixels.size()!=std::size_t(frame.width)*frame.height||filter<0||filter>2)throw std::runtime_error("Invalid GPU presentation input");
 RECT bounds{};if(!GetClientRect(window_,&bounds))throw std::runtime_error("Cannot read client bounds");if(bounds.right<=0||bounds.bottom<=0||IsIconic(window_))return false;resize(bounds.right,bounds.bottom);
 if(frame.width!=textureWidth_||frame.height!=textureHeight_){
  ID3D11ShaderResourceView* empty=nullptr;context_->PSSetShaderResources(0,1,&empty);imageView_.Reset();image_.Reset();D3D11_TEXTURE2D_DESC texture{};texture.Width=frame.width;texture.Height=frame.height;texture.MipLevels=texture.ArraySize=1;texture.Format=DXGI_FORMAT_B8G8R8A8_UNORM;texture.SampleDesc.Count=1;texture.Usage=D3D11_USAGE_DEFAULT;texture.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  check(device_->CreateTexture2D(&texture,nullptr,&image_),"Image texture");check(device_->CreateShaderResourceView(image_.Get(),nullptr,&imageView_),"Image view");textureWidth_=frame.width;textureHeight_=frame.height;
 }
 context_->UpdateSubresource(image_.Get(),0,nullptr,frame.pixels.data(),frame.width*4,0);
 const float clear[]={0,0,0,1};context_->ClearRenderTargetView(target_.Get(),clear);
 const unsigned dw=stretch?width_:unsigned(std::min(std::uint64_t(width_),std::uint64_t(height_)*4/3)),dh=stretch?height_:dw*3/4;
 const unsigned settings[]={frame.width,frame.height,unsigned(filter),0,(width_-dw)/2,(height_-dh)/2,dw,dh};context_->UpdateSubresource(constants_.Get(),0,nullptr,settings,0,0);
 if(dw&&dh){D3D11_VIEWPORT viewport{float((width_-dw)/2),float((height_-dh)/2),float(dw),float(dh),0,1};context_->RSSetViewports(1,&viewport);context_->RSSetState(rasterizer_.Get());
  const auto target=target_.Get();context_->OMSetRenderTargets(1,&target,nullptr);context_->IASetInputLayout(nullptr);context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context_->VSSetShader(vertex_.Get(),nullptr,0);context_->PSSetShader(pixel_.Get(),nullptr,0);const auto view=imageView_.Get();context_->PSSetShaderResources(0,1,&view);const auto cb=constants_.Get();context_->PSSetConstantBuffers(0,1,&cb);const auto sampler=linear_.Get();context_->PSSetSamplers(0,1,&sampler);context_->Draw(3,0);
 }
 return true;
}
bool NativeDxgiPresenter::present(){
 const DXGI_PRESENT_PARAMETERS parameters{};const auto hr=chain_->Present1(1,DXGI_PRESENT_DO_NOT_WAIT,&parameters);
 if(hr==DXGI_ERROR_WAS_STILL_DRAWING){++busy_;return false;}
 if(hr==DXGI_STATUS_OCCLUDED){occluded_=true;occlusionCheck_=GetTickCount64();token_=false;return false;}
 check(hr,"VSync Present1");token_=false;++presentations_;return true;
}
NativeFrame NativeDxgiPresenter::readback(){
 Ptr<ID3D11Texture2D> buffer;check(chain_->GetBuffer(0,IID_PPV_ARGS(&buffer)),"Readback buffer");D3D11_TEXTURE2D_DESC desc{};buffer->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;Ptr<ID3D11Texture2D> staging;check(device_->CreateTexture2D(&desc,nullptr,&staging),"Readback staging");context_->CopyResource(staging.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE mapped{};check(context_->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"Readback mapping");NativeFrame frame{width_,height_,std::vector<std::uint32_t>(std::size_t(width_)*height_)};for(unsigned y=0;y<height_;++y)std::memcpy(frame.pixels.data()+std::size_t(y)*width_,static_cast<const char*>(mapped.pData)+std::size_t(y)*mapped.RowPitch,width_*4);context_->Unmap(staging.Get(),0);return frame;
}
}
