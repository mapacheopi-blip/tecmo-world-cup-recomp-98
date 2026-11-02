#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <wrl/client.h>
#include "native_video.hpp"
#include <string>

namespace tecmo98 {
// One frame may be queued for display. Poll readiness without blocking the
// native game/audio/input thread; Present1 also uses DO_NOT_WAIT.
class NativeDxgiPresenter {
 template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
 HWND window_;HANDLE latency_=nullptr;bool token_=false,occluded_=false;
 unsigned width_=0,height_=0,textureWidth_=0,textureHeight_=0;
 std::uint64_t presentations_=0,busy_=0;ULONGLONG occlusionCheck_=0;
 Ptr<ID3D11Device> device_;Ptr<ID3D11DeviceContext> context_;
 Ptr<IDXGISwapChain2> chain_;Ptr<ID3D11RenderTargetView> target_;
 Ptr<ID3D11Texture2D> image_;Ptr<ID3D11ShaderResourceView> imageView_;
 Ptr<ID3D11VertexShader> vertex_;Ptr<ID3D11PixelShader> pixel_;
 Ptr<ID3D11Buffer> constants_;Ptr<ID3D11SamplerState> linear_;
 Ptr<ID3D11RasterizerState> rasterizer_;
 std::string adapter_;UINT flags_=DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
 void resize(unsigned,unsigned);
public:
 explicit NativeDxgiPresenter(HWND,bool warp=false);
 ~NativeDxgiPresenter();
 NativeDxgiPresenter(const NativeDxgiPresenter&)=delete;
 NativeDxgiPresenter& operator=(const NativeDxgiPresenter&)=delete;
 bool ready();
 bool draw(const NativeFrame&,bool stretch,int filter);
 // true only when a new present was accepted; false is a bounded retry.
 bool present();
 NativeFrame readback(); // Explicit verification only; never called per frame.
 bool statistics(DXGI_FRAME_STATISTICS& s)const{return SUCCEEDED(chain_->GetFrameStatistics(&s));}
 UINT lastPresent()const{UINT n=0;chain_->GetLastPresentCount(&n);return n;}
 const std::string& adapter()const{return adapter_;}
 auto presentations()const{return presentations_;}auto busy()const{return busy_;}
};
}
