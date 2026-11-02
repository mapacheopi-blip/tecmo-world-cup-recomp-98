// Explicit maintenance tool, never called by the product. Runtime changes only;
// caller must save first and restore in finally. Only enumerated same-size modes.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

struct SavedMode {
 DWORD signature=0x54434d44,version=1,size=sizeof(SavedMode);
 wchar_t device[32]{};
 DEVMODEW mode{};
};
static DISPLAY_DEVICEW primary(){
 DISPLAY_DEVICEW d{};d.cb=sizeof(d);
 for(DWORD i=0;EnumDisplayDevicesW(nullptr,i,&d,0);++i){
  if((d.StateFlags&(DISPLAY_DEVICE_PRIMARY_DEVICE|DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))==(DISPLAY_DEVICE_PRIMARY_DEVICE|DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))return d;
  d={};d.cb=sizeof(d);
 }
 throw std::runtime_error("No active primary display");
}
static DEVMODEW current(const wchar_t* device){
 DEVMODEW m{};m.dmSize=sizeof(m);
 if(!EnumDisplaySettingsExW(device,ENUM_CURRENT_SETTINGS,&m,0))throw std::runtime_error("Cannot query current mode");
 return m;
}
static bool sameSize(const DEVMODEW&a,const DEVMODEW&b){return a.dmPelsWidth==b.dmPelsWidth&&a.dmPelsHeight==b.dmPelsHeight&&a.dmBitsPerPel==b.dmBitsPerPel&&a.dmDisplayOrientation==b.dmDisplayOrientation&&a.dmDisplayFlags==b.dmDisplayFlags;}
static void apply(const wchar_t*device,DEVMODEW mode){
 const auto before=current(device);
 if(sameSize(before,mode)&&before.dmDisplayFrequency==mode.dmDisplayFrequency&&before.dmPosition.x==mode.dmPosition.x&&before.dmPosition.y==mode.dmPosition.y)return;
 mode.dmSize=sizeof(mode);mode.dmDriverExtra=0;
 // EnumDisplaySettings may return additional driver fields that cannot be
 // passed back to ChangeDisplaySettings. Request only the public mode fields.
 mode.dmFields=DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT|DM_DISPLAYFREQUENCY|DM_DISPLAYFLAGS|DM_DISPLAYORIENTATION|DM_POSITION;
 const LONG tested=ChangeDisplaySettingsExW(device,&mode,nullptr,CDS_TEST,nullptr);
 if(tested!=DISP_CHANGE_SUCCESSFUL)throw std::runtime_error("CDS_TEST rejected mode: "+std::to_string(tested));
 const LONG changed=ChangeDisplaySettingsExW(device,&mode,nullptr,0,nullptr);
 if(changed!=DISP_CHANGE_SUCCESSFUL)throw std::runtime_error("Runtime display change failed: "+std::to_string(changed));
 const auto actual=current(device);
 if(!sameSize(actual,mode)||actual.dmDisplayFrequency!=mode.dmDisplayFrequency)throw std::runtime_error("Display change readback mismatch");
}
int main(int argc,char**argv){try{
 if(argc<2)throw std::runtime_error("Use --list | --save FILE | --set HZ | --restore FILE");
 const std::string action=argv[1];auto d=primary();auto m=current(d.DeviceName);
 if(action=="--save"&&argc==3){
  SavedMode save;std::copy_n(d.DeviceName,32,save.device);save.mode=m;
  if(std::filesystem::exists(argv[2]))throw std::runtime_error("Will not overwrite saved original mode");
  std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<const char*>(&save),sizeof(save));if(!out)throw std::runtime_error("Cannot save mode");
 }else if(action=="--restore"&&argc==3){
  SavedMode save;std::ifstream in(argv[2],std::ios::binary);in.read(reinterpret_cast<char*>(&save),sizeof(save));
  if(!in||in.peek()!=EOF||save.signature!=0x54434d44||save.version!=1||save.size!=sizeof(save)||save.mode.dmSize!=sizeof(DEVMODEW)||save.device[31]!=0)throw std::runtime_error("Invalid saved mode");
  if(wcscmp(save.device,d.DeviceName)!=0)throw std::runtime_error("Saved primary display is no longer primary");
  apply(save.device,save.mode);m=current(d.DeviceName);
 }else if(action=="--set"&&argc==3){
  const auto hz=std::stoul(argv[2]);bool found=false;
  for(DWORD i=0;;++i){DEVMODEW candidate{};candidate.dmSize=sizeof(candidate);if(!EnumDisplaySettingsExW(d.DeviceName,i,&candidate,0))break;
   if(sameSize(candidate,m)&&candidate.dmDisplayFrequency==hz){candidate.dmPosition=m.dmPosition;candidate.dmFields|=DM_POSITION;apply(d.DeviceName,candidate);found=true;break;}}
  if(!found)throw std::runtime_error("No monitor-supported mode at that Hz with current size, color and orientation");
  m=current(d.DeviceName);
 }else if(action!="--list"||argc!=2)throw std::runtime_error("Invalid arguments");
 std::set<DWORD> rates;for(DWORD i=0;;++i){DEVMODEW v{};v.dmSize=sizeof(v);if(!EnumDisplaySettingsExW(d.DeviceName,i,&v,0))break;if(sameSize(v,m))rates.insert(v.dmDisplayFrequency);}
 DWM_TIMING_INFO timing{};timing.cbSize=sizeof(timing);const bool dwm=SUCCEEDED(DwmGetCompositionTimingInfo(nullptr,&timing));
 std::cout<<"{\"width\":"<<m.dmPelsWidth<<",\"height\":"<<m.dmPelsHeight<<",\"bpp\":"<<m.dmBitsPerPel<<",\"hz\":"<<m.dmDisplayFrequency<<",\"dwm_valid\":"<<(dwm?"true":"false")<<",\"dwm_numerator\":"<<timing.rateRefresh.uiNumerator<<",\"dwm_denominator\":"<<timing.rateRefresh.uiDenominator<<",\"supported_hz\":[";
 bool first=true;for(auto hz:rates){if(!first)std::cout<<',';std::cout<<hz;first=false;}std::cout<<"]}\n";
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
