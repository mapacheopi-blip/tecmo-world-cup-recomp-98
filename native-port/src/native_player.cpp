#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <cmath>
#include "native_launcher.hpp"
#include "native_session.hpp"
#include "native_frame_composer.hpp"
#include "native_presentation_clock.hpp"
#include "native_gdi_presenter.hpp"
#include "native_dxgi_presenter.hpp"
#include "native_host_phases.hpp"
#include "native_contact_mailbox.hpp"
#include <atomic>
#include <thread>
#include <exception>
#include <dwmapi.h>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <list>
#include <deque>
#include <cstdlib>

namespace {
tecmo98::NativeFrame frame,presentationFrame;
tecmo98::NativeGdiPresenter presenter;std::string presentationError;
std::unique_ptr<tecmo98::NativeDxgiPresenter> gpuPresenter;
std::atomic<bool> running{true};
std::array<bool,256> keyEvents{};
std::vector<std::int16_t> recorded;
std::uint64_t paintedFrames=0;std::vector<double> paintIntervals;
std::chrono::steady_clock::time_point lastPaint{};
std::ofstream framePerformance,paintPerformance,displayPerformance;
std::chrono::steady_clock::time_point visibleSubmitted{};std::uint64_t visibleSequence=0;
std::uint64_t visibleSourceFrame=0;
long long hostNanoseconds(std::chrono::steady_clock::time_point t){return std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count();}
void saveWave(){std::ofstream out("native-audio.wav",std::ios::binary);const auto le=[&](unsigned value,unsigned n){for(unsigned i=0;i<n;++i)out.put(char(value>>(i*8)));};out.write("RIFF",4);le(36+unsigned(recorded.size()*2),4);out.write("WAVEfmt ",8);le(16,4);le(1,2);le(2,2);le(44100,4);le(176400,4);le(4,2);le(16,2);out.write("data",4);le(unsigned(recorded.size()*2),4);out.write(reinterpret_cast<const char*>(recorded.data()),recorded.size()*2);}
class Audio {
    HWAVEOUT device_=nullptr;double gain_=1.0;
    struct Block{WAVEHDR header{};std::vector<std::int16_t> samples;};std::list<Block> pending_;
    std::vector<std::int16_t> staging_;std::size_t queuedFrames_=0;bool started_=false;
    std::uint64_t submitted_=0,underruns_=0;std::size_t peakQueue_=0;
    void reap(){while(!pending_.empty()&&(pending_.front().header.dwFlags&WHDR_DONE)!=0){auto& b=pending_.front();queuedFrames_-=b.samples.size()/2;waveOutUnprepareHeader(device_,&b.header,sizeof(b.header));pending_.pop_front();}}
public:
    Audio(double gain=1.0,bool muted=false):gain_(muted?0.0:gain){WAVEFORMATEX format{WAVE_FORMAT_PCM,2,44100,176400,4,16,0};if(waveOutOpen(&device_,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR)throw std::runtime_error("Cannot open Windows PCM audio output");
        if(waveOutPause(device_)!=MMSYSERR_NOERROR){waveOutClose(device_);device_=nullptr;throw std::runtime_error("Cannot prime Windows PCM output");}
        UINT id=0;WAVEOUTCAPSA caps{};
        if(waveOutGetID(device_,&id)==MMSYSERR_NOERROR&&waveOutGetDevCapsA(id,&caps,sizeof(caps))==MMSYSERR_NOERROR)
            std::cout<<"Audio output: "<<caps.szPname<<" (44100 Hz, stereo PCM16)"<<std::endl;
    }
    ~Audio(){if(device_){waveOutReset(device_);for(auto& b:pending_)waveOutUnprepareHeader(device_,&b.header,sizeof(b.header));waveOutClose(device_);}}
    void submit(std::vector<std::int16_t> samples){
        reap();if(started_&&pending_.empty()){++underruns_;started_=false;if(waveOutPause(device_)!=MMSYSERR_NOERROR)throw std::runtime_error("Cannot rebuffer Windows PCM output");}
        if(gain_!=1.0)for(auto& sample:samples)sample=std::int16_t(std::lround(double(sample)*gain_));
        staging_.insert(staging_.end(),samples.begin(),samples.end());std::size_t consumed=0;
        constexpr std::size_t blockSamples=441*2; // 10 ms stereo blocks, 80 ms initial reserve.
        while(staging_.size()-consumed>=blockSamples){
            if(queuedFrames_>22050)throw std::runtime_error("Native audio output queue exceeds 500 ms");
            pending_.emplace_back();auto& b=pending_.back();b.samples.assign(staging_.begin()+consumed,staging_.begin()+consumed+blockSamples);consumed+=blockSamples;
            b.header.lpData=reinterpret_cast<char*>(b.samples.data());b.header.dwBufferLength=DWORD(b.samples.size()*2);
            if(waveOutPrepareHeader(device_,&b.header,sizeof(b.header))!=MMSYSERR_NOERROR||waveOutWrite(device_,&b.header,sizeof(b.header))!=MMSYSERR_NOERROR)throw std::runtime_error("Windows PCM audio submission failed");
            queuedFrames_+=441;submitted_+=441;peakQueue_=std::max(peakQueue_,queuedFrames_);
        }
        if(consumed)staging_.erase(staging_.begin(),staging_.begin()+consumed);
        if(!started_&&queuedFrames_>=3528){if(waveOutRestart(device_)!=MMSYSERR_NOERROR)throw std::runtime_error("Cannot start buffered Windows PCM output");started_=true;}
    }
    auto underruns()const{return underruns_;}auto submitted()const{return submitted_;}
    double peakMilliseconds()const{return double(peakQueue_)*1000/44100;}
};
LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM w,LPARAM l){
    if(message==WM_DESTROY){running=false;PostQuitMessage(0);return 0;}
    if(message==WM_KEYDOWN&&w<256&&(l&(1ull<<30))==0)keyEvents[w]=true;
    if(message==WM_KILLFOCUS)keyEvents.fill(false);
    if(message==WM_KEYDOWN&&w==VK_ESCAPE){DestroyWindow(window);return 0;}
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_PAINT){if(gpuPresenter){PAINTSTRUCT paint;BeginPaint(window,&paint);EndPaint(window,&paint);return 0;}const auto stamp=std::chrono::steady_clock::now();if(lastPaint.time_since_epoch().count()!=0&&paintIntervals.size()<30000)paintIntervals.push_back(std::chrono::duration<double,std::milli>(stamp-lastPaint).count());lastPaint=stamp;++paintedFrames;PAINTSTRUCT paint;const auto dc=BeginPaint(window,&paint);RECT area;GetClientRect(window,&area);
        if(!presenter.paint(dc,area.right,area.bottom,presentationFrame.width?presentationFrame:frame)){presentationError="Native GDI buffered presentation failed";running=false;PostQuitMessage(0);}
        EndPaint(window,&paint);if(paintPerformance.is_open())paintPerformance<<hostNanoseconds(stamp)<<","<<visibleSequence<<","<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-stamp).count()<<","<<(visibleSequence?std::chrono::duration<double,std::milli>(stamp-visibleSubmitted).count():0)<<"\n";return 0;}
    return DefWindowProcW(window,message,w,l);
}
void saveBitmap(const std::filesystem::path& path){
    std::ofstream file(path,std::ios::binary);BITMAPFILEHEADER h{};h.bfType=0x4d42;h.bfOffBits=sizeof(h)+sizeof(BITMAPINFOHEADER);h.bfSize=h.bfOffBits+DWORD(frame.pixels.size()*4);
    BITMAPINFOHEADER info{sizeof(BITMAPINFOHEADER),LONG(frame.width),-LONG(frame.height),1,32,BI_RGB,DWORD(frame.pixels.size()*4),0,0,0,0};
    file.write(reinterpret_cast<char*>(&h),sizeof(h));file.write(reinterpret_cast<char*>(&info),sizeof(info));file.write(reinterpret_cast<const char*>(frame.pixels.data()),frame.pixels.size()*4);
}
void dumpVideo(const tecmo98::NativeVideoView& v){
    for(const auto& [name,data]:std::array<std::pair<const char*,std::span<const std::uint8_t>>,6>{{{"native-display.bin",v.display},{"native-draw.bin",v.draw},{"native-vdp2.bin",v.vram},{"native-cram.bin",v.color},{"native-vdp1-regs.bin",v.vdp1},{"native-vdp2-regs.bin",v.vdp2}}}){std::ofstream out(name,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());}
}
// Same SDL3 HIDAPI/XInput/DirectInput backend already used by the launcher.
// Only the device API is loaded: all game code remains statically compiled.
class Controllers {
    HMODULE library_=nullptr;
    template<class T>T symbol(const char* name){const auto p=GetProcAddress(library_,name);if(!p)throw std::runtime_error(std::string("Missing SDL3 export: ")+name);return reinterpret_cast<T>(p);}
    bool (*init_)(std::uint32_t);void (*quit_)();void (*pump_)();std::uint32_t*(*ids_)(int*);void (*free_)(void*);
    bool (*isPad_)(std::uint32_t);void*(*open_)(std::uint32_t);void(*close_)(void*);bool(*button_)(void*,int);std::int16_t(*axis_)(void*,int);
    const char*(*name_)(void*);std::map<std::uint32_t,void*> pads_;
    std::array<std::uint64_t,256> keyUntil_{};
public:
    explicit Controllers(const std::filesystem::path& path){library_=LoadLibraryW(path.c_str());if(!library_)throw std::runtime_error("SDL3.dll is missing beside the native executable");
        init_=symbol<decltype(init_)>("SDL_Init");quit_=symbol<decltype(quit_)>("SDL_Quit");pump_=symbol<decltype(pump_)>("SDL_PumpEvents");ids_=symbol<decltype(ids_)>("SDL_GetGamepads");free_=symbol<decltype(free_)>("SDL_free");isPad_=symbol<decltype(isPad_)>("SDL_IsGamepad");open_=symbol<decltype(open_)>("SDL_OpenGamepad");close_=symbol<decltype(close_)>("SDL_CloseGamepad");button_=symbol<decltype(button_)>("SDL_GetGamepadButton");axis_=symbol<decltype(axis_)>("SDL_GetGamepadAxis");name_=symbol<decltype(name_)>("SDL_GetGamepadName");
        const auto hint=symbol<bool(*)(const char*,const char*)>("SDL_SetHint");hint("SDL_JOYSTICK_HIDAPI_PS5","1");hint("SDL_JOYSTICK_HIDAPI","1");hint("SDL_JOYSTICK_RAWINPUT","1");hint("SDL_JOYSTICK_DIRECTINPUT","1");
        if(!init_(0x2200))throw std::runtime_error("SDL3 controller initialization failed");}
    ~Controllers(){for(const auto& [id,p]:pads_){(void)id;close_(p);}if(library_){quit_();FreeLibrary(library_);}}
    std::array<std::uint8_t,7> poll(bool focused){pump_();int count=0;auto ids=ids_(&count);std::map<std::uint32_t,void*> present;
        for(int i=0;i<count;++i){const auto id=ids[i];auto p=pads_.contains(id)?pads_.at(id):open_(id);if(p){present[id]=p;if(!pads_.contains(id))std::cout<<"Controller: "<<name_(p)<<std::endl;}}
        free_(ids);for(const auto& [id,p]:pads_)if(!present.contains(id))close_(p);pads_=std::move(present);
        std::array<unsigned,2> contacts{};unsigned system=0,index=0;
        if(focused)for(const auto& [id,p]:pads_){(void)id;if(index>=2)break;const auto b=[&](int n){return button_(p,n);};const auto ax=axis_(p,0),ay=axis_(p,1);
            auto& v=contacts[index];if(b(11)||ay< -14745)v|=0x20;if(b(12)||ay>14745)v|=0x10;if(b(13)||ax< -14745)v|=0x80;if(b(14)||ax>14745)v|=0x40;
            for(int n=0;n<4;++n)if(b(n))v|=1u<<n;if(b(4))system|=1u<<index;if(b(6))system|=0x10u<<index;++index;}
        const auto gameNow=GetTickCount64()*1000000ull;for(unsigned k=0;k<keyEvents.size();++k){if(focused&&keyEvents[k])keyUntil_[k]=gameNow+100000000ull;keyEvents[k]=false;if(!focused)keyUntil_[k]=0;}
        const auto key=[&](int k){return focused&&((GetAsyncKeyState(k)&0x8000)!=0||gameNow<keyUntil_[unsigned(k)]);};
        if(key(VK_UP))contacts[0]|=0x20;if(key(VK_DOWN))contacts[0]|=0x10;if(key(VK_LEFT))contacts[0]|=0x80;if(key(VK_RIGHT))contacts[0]|=0x40;
        if(key('Z'))contacts[0]|=1;if(key('X'))contacts[0]|=2;if(key('C'))contacts[0]|=4;if(key('V'))contacts[0]|=8;
        if(key('5'))system|=1;if(key('6'))system|=2;if(key('1')||key(VK_RETURN))system|=0x10;if(key('2'))system|=0x20;
        for(auto& c:contacts){if((c&0x30)==0x30)c&=~0x30u;if((c&0xc0)==0xc0)c&=~0xc0u;}return {std::uint8_t(contacts[0]),std::uint8_t(contacts[1]),std::uint8_t(system),0,0,0,0};
    }
};
}
int main(int argc,char** argv){
    try{
        if(argc==2&&std::string(argv[1])=="--replay"){
            std::array<std::vector<std::uint8_t>,6> buffers;unsigned i=0;
            for(const auto& [name,size]:std::array<std::pair<const char*,unsigned>,6>{{{"native-display.bin",0x40000},{"native-draw.bin",0x40000},{"native-vdp2.bin",0x80000},{"native-cram.bin",0x1000},{"native-vdp1-regs.bin",32},{"native-vdp2-regs.bin",512}}}){auto& b=buffers[i++];b.resize(size);std::ifstream in(name,std::ios::binary);if(!in.read(reinterpret_cast<char*>(b.data()),size)||in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Invalid video snapshot");}
            frame=tecmo98::compose_native_frame({buffers[0],buffers[1],buffers[2],buffers[3],buffers[4],buffers[5]});saveBitmap("native-replayed-frame.bmp");return 0;
        }
        if(argc!=5&&argc!=7){std::cerr<<"<EPROM> <raw jp1 BIOS> <mapped cart> <resident profile> [--smoke seconds]\n";return 2;}
        const bool windowSmoke=argc==7&&std::string(argv[5])=="--smoke-window";const bool audioSmoke=argc==7&&std::string(argv[5])=="--smoke-audio";const bool smoke=argc==7&&(std::string(argv[5])=="--smoke"||audioSmoke||windowSmoke);const double duration=smoke?std::stod(argv[6]):0;if(argc==7&&(!smoke||duration<=0||duration>300))throw std::invalid_argument("Invalid smoke duration");
        int wideCount=0;auto wideArgs=CommandLineToArgvW(GetCommandLineW(),&wideCount);
        if(!wideArgs||wideCount!=argc)throw std::runtime_error("Cannot read native Unicode arguments");
        std::array<std::filesystem::path,5> paths;for(int i=0;i<5;++i)paths[i]=wideArgs[i];LocalFree(wideArgs);
        tecmo98::LaunchSettings settings;
        if(const auto config=_wgetenv(L"TECMO98_LAUNCHER_CONFIG")){std::ifstream in{std::filesystem::path(config)};settings=tecmo98::LaunchSettings::read(in);}
        presenter.configure(settings.stretch!=0,settings.smooth==1?1:0);
        std::filesystem::path inputFile;if(const auto file=_wgetenv(L"TECMO98_INPUT_FILE"))inputFile=file;
        tecmo98::LauncherInput launcherInput;
        auto session=std::make_unique<tecmo98::NativeSession>(paths[1],paths[2],paths[3],paths[4]);HWND window=nullptr;std::unique_ptr<Controllers> controls;std::unique_ptr<Audio> audio;
        if(!smoke||windowSmoke){WNDCLASSW cls{};cls.lpfnWndProc=windowProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"Tecmo98Native";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
            window=CreateWindowExW(0,cls.lpszClassName,L"TECMO WORLD CUP RECOMP 98 · Desarrollo nativo · 5 crédito / 1 inicio / flechas / Z X C",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,settings.width,settings.height,nullptr,nullptr,cls.hInstance,nullptr);
            if(windowSmoke)SetWindowTextW(window,L"TECMO WORLD CUP RECOMP 98 · Prueba automática de rendimiento y audio");
            if(!window)throw std::runtime_error("Native window creation failed");
            if(settings.fullscreen){MONITORINFO monitor{sizeof(MONITORINFO)};if(!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor))throw std::runtime_error("Cannot read fullscreen monitor");SetWindowLongPtrW(window,GWL_STYLE,WS_POPUP|WS_VISIBLE);SetWindowPos(window,HWND_TOP,monitor.rcMonitor.left,monitor.rcMonitor.top,monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,SWP_FRAMECHANGED|SWP_SHOWWINDOW);}
            else {RECT bounds{0,0,settings.width,settings.height};AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);SetWindowPos(window,nullptr,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOMOVE|SWP_NOZORDER);}
            if(!inputFile.empty())SetWindowTextW(window,L"TECMO WORLD CUP RECOMP 98 · Esc para volver al launcher");
            else controls=std::make_unique<Controllers>(std::filesystem::absolute(paths[0]).parent_path()/"SDL3.dll");
            audio=std::make_unique<Audio>(settings.gain(),settings.muted!=0);}
        if(audioSmoke&&!audio)audio=std::make_unique<Audio>(settings.gain(),settings.muted!=0);
        if(window&&!std::getenv("TECMO98_FORCE_GDI"))try{gpuPresenter=std::make_unique<tecmo98::NativeDxgiPresenter>(window);std::cout<<"Presentation: Direct3D 11 flip / VSync / latency 1 / "<<gpuPresenter->adapter()<<std::endl;}catch(const std::exception& e){std::cerr<<"GPU initialization fallback to GDI: "<<e.what()<<std::endl;}
        DWM_TIMING_INFO desktopTiming{};desktopTiming.cbSize=sizeof(desktopTiming);if(SUCCEEDED(DwmGetCompositionTimingInfo(nullptr,&desktopTiming)))std::cout<<"Desktop refresh: "<<desktopTiming.rateRefresh.uiNumerator<<"/"<<desktopTiming.rateRefresh.uiDenominator<<" Hz"<<std::endl;
        std::ofstream videoTrace;if(std::getenv("TECMO98_VIDEO_TRACE")){videoTrace.open("native-video-frames.csv");videoTrace<<"sequence,game_ns,video_mode,layers,black_fraction,main_pc\n";}
        if(std::getenv("TECMO98_PERF_TRACE")){framePerformance.open("native-performance-frames.csv");framePerformance<<"host_ns,sequence,game_ns,draw_lists,compose_ms,filter_ms,queue_ms,submit_ms,core_max_ms,motion_fraction,source_frame\n";paintPerformance.open("native-performance-paints.csv");paintPerformance<<"host_ns,sequence,paint_ms,frame_age_ms\n";}
        tecmo98::NativeFrameComposer composer(gpuPresenter?0:settings.smooth);std::uint64_t presentedFrames=0,coreNs=0,lastGpuSequence=0,preparedGpuSequence=0;std::atomic<double> coreMaxMs{0};std::atomic<std::uint64_t> completedDrawLists{0};std::atomic<unsigned> mainPc{0};std::vector<std::uint32_t> previousMotion;
        session->videoFrameReady=[&](const tecmo98::NativeVideoView& view,std::uint64_t time,std::uint64_t sourceFrame){composer.submit(view,time,sourceFrame);};
        if(gpuPresenter&&std::getenv("TECMO98_PERF_TRACE")){displayPerformance.open("native-dxgi-frames.csv");displayPerformance<<"host_ns,sequence,source_frame,last_present,stats_valid,displayed_present,displayed_refresh,sync_refresh,sync_qpc\n";}
        std::ofstream events("native-interrupt-events.txt");if(std::getenv("TECMO98_GATES"))session->interruptTrace=[&](bool slave,int source,unsigned vector,unsigned target,unsigned pc){events<<"I "<<(slave?"S ":"M ")<<std::hex<<source<<" "<<vector<<" "<<target<<" "<<pc<<"\n";};
        struct Store{bool slave;unsigned pc,address,size;std::uint64_t value;};std::deque<Store> stores,gates;if(std::getenv("TECMO98_TRACE")||std::getenv("TECMO98_GATES"))session->dataWriteTrace=[&](bool slave,unsigned pc,unsigned a,unsigned size,std::uint64_t value){if(a<=0x605d370&&std::uint64_t(a)+size>0x605d370&&session->gameNanoseconds()>2000000000ull)throw std::runtime_error("Diagnostic: write into compiled code from PC "+std::to_string(pc));if((a>=0x60ffc10&&a<0x60ffc14)||(a>=0x60ffca8&&a<=0x60ffcb0)){if(gates.size()==500000)gates.pop_front();gates.push_back({slave,pc,a,size,value});events<<"W "<<(slave?"S ":"M ")<<std::hex<<pc<<" "<<a<<" "<<size<<" "<<value<<"\n";}if((a>=0x60c7000&&a<0x60cd000)||(a>=0x60f1e00&&a<0x60f3000)){if(stores.size()==8192)stores.pop_front();stores.push_back({slave,pc,a,size,value});}};
        const bool traceEnabled=std::getenv("TECMO98_TRACE")!=nullptr;unsigned sortStarted=0,sortReady=0,frameStarts=0;std::deque<std::pair<bool,unsigned>> trace;session->sourceTrace=[&](bool slave,unsigned pc){if(slave&&pc==0x606427c)++sortStarted;if(slave&&pc==0x6064330)++sortReady;if(!slave&&pc==0x6004168)++frameStarts;if(traceEnabled&&(pc==0x6058406||pc==0x6058410||pc==0x6058424)){const auto& c=slave?session->slaveContext:session->context;std::cerr<<"DIV "<<(slave?"S ":"M ")<<std::hex<<pc<<" r4="<<c.reg(4)<<" r5="<<c.reg(5)<<" den="<<session->read(0xffffff00,4)<<" q="<<session->read(0xffffff1c,4)<<" ovf="<<session->read(0xffffff08,4)<<std::dec<<std::endl;}if(traceEnabled&&!slave&&pc==0x60583e0)std::cerr<<"ATAN x="<<std::hex<<session->context.reg(4)<<" y="<<session->context.reg(5)<<std::dec<<std::endl; if(traceEnabled&&!slave&&(pc==0x6036b8a||pc==0x6036b9a||pc==0x6036bca)){std::cout<<"State monitor PC="<<std::hex<<pc<<" prev="<<session->context.reg(13)<<" ptr="<<session->context.reg(14)<<" seen="<<session->context.reg(5)<<" next="<<session->context.reg(4)<<" current="<<session->read(std::uint32_t(session->read(session->context.reg(14),4))+6,1)<<std::dec<<std::endl;}if(!traceEnabled)return;if(trace.size()==4096)trace.pop_front();trace.emplace_back(slave,pc);};
        std::ofstream inputTrace;if(std::getenv("TECMO98_INPUT_TRACE")){inputTrace.open("native-input-reads.csv");inputTrace<<"host_ms,game_ns,kind,port,value\n";}
        std::array<unsigned,64> ioCounts{},ioPressed{};session->ioReadTrace=[&](unsigned a,std::uint8_t value){++ioCounts[a&63];ioPressed[a&63]|=unsigned(std::uint8_t(~value));if(inputTrace.is_open()&&((a&63)==1||(a&63)==3||(a&63)==5||(a&63)==9||(a&63)==11))inputTrace<<GetTickCount64()<<","<<session->gameNanoseconds()<<",read,"<<(a&63)<<","<<unsigned(value)<<"\n";};
        std::array<std::uint8_t,7> previousContacts{};
        const auto gameEpoch=session->gameNanoseconds();const auto epoch=std::chrono::steady_clock::now();double inputServiceMaxMs=0;double elapsed=0;unsigned capturedBitmap=0;bool frameComplete=false;std::string compositionError;
        tecmo98::HostPhases hostPhases(std::getenv("TECMO98_PERF_TRACE")!=nullptr),gamePhases(std::getenv("TECMO98_PERF_TRACE")!=nullptr),filePhases(std::getenv("TECMO98_PERF_TRACE")!=nullptr);
        tecmo98::NativeContactMailbox directContacts;tecmo98::NativePacketMailbox inputPackets;std::exception_ptr gameFailure;
        // Local file I/O can stall on Windows filters/atomic replacement. A
        // dedicated reader owns it; neither rendering nor native audio waits.
        std::jthread inputThread;
        if(!inputFile.empty())inputThread=std::jthread([&](std::stop_token stop){while(running.load()&&!stop.stop_requested()){
            {tecmo98::HostPhases::Scope phase(filePhases,tecmo98::HostPhases::InputFile);
            const auto handle=CreateFileW(inputFile.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(handle!=INVALID_HANDLE_VALUE){std::array<char,1024> bytes{};DWORD count=0;const bool ok=ReadFile(handle,bytes.data(),DWORD(bytes.size()),&count,nullptr)!=0;CloseHandle(handle);if(ok&&count<bytes.size())inputPackets.publish(std::string(bytes.data(),count));}}
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }});
        // The session and audio have exactly one owner. UI/DirectX never access
        // live game memory; they consume only the composer's immutable results.
        std::jthread gameThread([&](std::stop_token stop){try{
            auto serviced=epoch;unsigned captured=0;
            while(running.load()&&!stop.stop_requested()){
                const auto now=std::chrono::steady_clock::now();const double elapsed=std::chrono::duration<double>(now-epoch).count();
                if(smoke&&elapsed>duration){running=false;break;}
            if(now-serviced>=std::chrono::milliseconds(5)){inputServiceMaxMs=std::max(inputServiceMaxMs,std::chrono::duration<double,std::milli>(now-serviced).count());
                {tecmo98::HostPhases::Scope phase(gamePhases,tecmo98::HostPhases::AudioOutput);auto samples=session->takeAudio();if(smoke)recorded.insert(recorded.end(),samples.begin(),samples.end());if(audio)audio->submit(std::move(samples));}
                if(controls){const auto contacts=directContacts.read(GetTickCount64(),settings.background||GetForegroundWindow()==window);for(unsigned p=0;p<contacts.size();++p)session->setInputContacts(p,contacts[p]);}
                if(!inputFile.empty()){
                    const auto stamp=GetTickCount64();std::istringstream in(inputPackets.read());launcherInput.accept(in,stamp);
                    if(launcherInput.quit()){running=false;break;}
                    const auto contacts=launcherInput.contacts(stamp,settings.background||GetForegroundWindow()==window);
                    for(unsigned p=0;p<contacts.size();++p){session->setInputContacts(p,contacts[p]);if(inputTrace.is_open()&&contacts[p]!=previousContacts[p])inputTrace<<stamp<<","<<session->gameNanoseconds()<<",set,"<<p<<","<<unsigned(contacts[p])<<"\n";}previousContacts=contacts;
                }
                if(smoke){const double game=double(session->gameNanoseconds()-gameEpoch)/1e9;session->setInputContacts(2,game>=2&&game<2.2?1:game>=4&&game<4.2?0x10:0);
                    unsigned p1=game>=8&&game<8.2?1:0;
                    if(game>=18){const auto phase=unsigned((game-18)/3)%4;p1|=phase==0?0x40:phase==1?0x20:phase==2?0x80:0x10;if(int(game*2)%4==0)p1|=1;}
                    session->setInputContacts(0,std::uint8_t(p1));}
                serviced=now;
            }
                if(smoke&&unsigned(elapsed/5)>captured){captured=unsigned(elapsed/5);const auto stem="native-frame-"+std::to_string(captured*5);dumpVideo(session->videoView());{const auto v=session->videoView();std::ofstream cfg(stem+".regs",std::ios::binary);cfg.write(reinterpret_cast<const char*>(v.vdp2.data()),v.vdp2.size());}std::cout<<"Native frame "<<captured*5<<" main PC="<<std::hex<<session->context.pc<<" io="<<ioCounts[5]<<" pressed="<<ioPressed[5]<<" p1="<<ioPressed[1]<<" SR="<<session->context.special(0x100)<<" game="<<std::dec<<double(session->gameNanoseconds()-gameEpoch)/1e9<<std::hex<<" ticks="<<frameStarts<<" sorts="<<sortStarted<<":"<<sortReady<<" state="<<session->read(0x060ffca8,4)<<" gate="<<session->read(0x060ffc10,4)<<" buffers="<<session->read(0x060ffc30,4)<<":"<<session->read(0x060ffc34,4)<<" queue="<<session->read(0x060ffc44,4)<<":"<<session->read(0x060ffc48,4)<<" sound="<<session->soundContext.pc<<std::dec<<std::endl;}
            const auto coreStart=std::chrono::steady_clock::now();try{tecmo98::HostPhases::Scope phase(gamePhases,tecmo98::HostPhases::Core);session->runCooperative(8192);const auto coreCost=std::chrono::steady_clock::now()-coreStart;coreNs+=std::chrono::duration_cast<std::chrono::nanoseconds>(coreCost).count();const double cost=std::chrono::duration<double,std::milli>(coreCost).count();double previous=coreMaxMs.load();while(previous<cost&&!coreMaxMs.compare_exchange_weak(previous,cost)){}completedDrawLists.store(session->completedDrawLists());mainPc.store(session->context.pc);}catch(const std::exception& error){for(const auto& [name,data]:std::array<std::pair<const char*,std::span<const std::uint8_t>>,2>{{{"native-scsp.bin",{}},{"native-vdp2-regs.bin",session->videoView().vdp2}}}){std::ofstream out(name,std::ios::binary);if(!data.empty())out.write(reinterpret_cast<const char*>(data.data()),data.size());else for(unsigned a=0;a<0x1000;a+=2){auto v=session->read(0x05b00000+a,2);out.put(char(v>>8));out.put(char(v));}}{std::ofstream f("native-player-stores.txt");for(auto s:stores)f<<(s.slave?"S ":"M ")<<std::hex<<s.pc<<" "<<s.address<<" "<<s.size<<" "<<s.value<<"\n";}{std::ofstream f("native-player-trace.txt");for(auto [slave,pc]:trace)f<<(slave?"S ":"M ")<<std::hex<<pc<<"\n";for(const auto* c:{&session->context,&session->slaveContext}){f<<"PC="<<std::hex<<c->pc;for(unsigned i=0;i<16;++i)f<<" R"<<std::dec<<i<<"="<<std::hex<<c->reg(i);f<<"\n";}}{std::ofstream out("native-workram.bin",std::ios::binary);for(unsigned a=0;a<0x100000;a+=4){auto value=session->read(0x06000000+a,4);for(int shift=24;shift>=0;shift-=8)out.put(char(value>>shift));}}dumpVideo(session->videoView());throw std::runtime_error(std::string(error.what())+" at main PC "+std::to_string(session->context.pc)+" sound PC "+std::to_string(session->soundContext.pc));}
            }
        }catch(...){gameFailure=std::current_exception();running=false;}});
        auto controllersPolled=epoch-std::chrono::milliseconds(5);
        struct TimerResolution {bool active=timeBeginPeriod(1)==TIMERR_NOERROR;~TimerResolution(){if(active)timeEndPeriod(1);}} timerResolution;
        bool testStalled=false;unsigned testStallMs=0;
        if(const auto value=std::getenv("TECMO98_TEST_UI_STALL_MS"))testStallMs=std::min(250ul,std::strtoul(value,nullptr,10));
        while(running){
            {tecmo98::HostPhases::Scope phase(hostPhases,tecmo98::HostPhases::Messages);MSG message;for(unsigned messages=0;messages<64&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++messages){TranslateMessage(&message);DispatchMessageW(&message);}}
            if(!running)break;
            const auto now=std::chrono::steady_clock::now();elapsed=std::chrono::duration<double>(now-epoch).count();
            if(testStallMs&&!testStalled&&elapsed>=25){testStalled=true;Sleep(testStallMs);std::cout<<"Test UI pause: "<<testStallMs<<" ms; native game thread remains independent"<<std::endl;}
            if(controls&&now-controllersPolled>=std::chrono::milliseconds(5)){tecmo98::HostPhases::Scope phase(hostPhases,tecmo98::HostPhases::Controllers);directContacts.publish(controls->poll(settings.background||GetForegroundWindow()==window),GetTickCount64());controllersPolled=now;}
            if(auto result=composer.take()){tecmo98::HostPhases::Scope phase(hostPhases,tecmo98::HostPhases::FrameResult);
                if(result->error.empty()){visibleSequence=result->sequence;visibleSourceFrame=result->sourceFrame;visibleSubmitted=result->submitted;}
                if(framePerformance.is_open()&&result->error.empty()){std::vector<std::uint32_t> motion;for(std::size_t i=0;i<result->frame.pixels.size();i+=67)motion.push_back(result->frame.pixels[i]);unsigned changed=0;if(motion.size()==previousMotion.size())for(std::size_t i=0;i<motion.size();++i)changed+=motion[i]!=previousMotion[i];framePerformance<<hostNanoseconds(std::chrono::steady_clock::now())<<","<<result->sequence<<","<<result->time<<","<<completedDrawLists.load()<<","<<result->composeMs<<","<<result->filterMs<<","<<result->queueMs<<","<<result->submitMs<<","<<coreMaxMs.exchange(0)<<","<<(motion.empty()?0:double(changed)/motion.size())<<","<<result->sourceFrame<<"\n";previousMotion=std::move(motion);}
                frameComplete=result->error.empty();if(frameComplete){composer.recycleFrames(std::move(frame),std::move(presentationFrame));frame=std::move(result->frame);presentationFrame=std::move(result->presentation);compositionError.clear();++presentedFrames;if(videoTrace.is_open()){unsigned black=0,count=0;for(std::size_t p=0;p<frame.pixels.size();p+=29){++count;if((frame.pixels[p]&0xffffff)==0)++black;}const double fraction=count?double(black)/count:1;videoTrace<<result->sequence<<","<<result->time<<","<<result->videoMode<<","<<result->layers<<","<<fraction<<","<<mainPc.load()<<"\n";if(fraction>0.99&&elapsed>5)saveBitmap("native-near-black-"+std::to_string(result->sequence)+".bmp");}}
                else {if(compositionError!=result->error)std::cerr<<"Composition: "<<result->error<<"\n";compositionError=result->error;}
                if(window)InvalidateRect(window,nullptr,FALSE);
            }
            if(gpuPresenter&&frame.width&&visibleSequence!=lastGpuSequence&&!IsIconic(window))try{tecmo98::HostPhases::Scope phase(hostPhases,tecmo98::HostPhases::Gpu);
                if(gpuPresenter->ready()){
                    const auto stamp=std::chrono::steady_clock::now();
                    if(preparedGpuSequence!=visibleSequence&&gpuPresenter->draw(frame,settings.stretch!=0,settings.smooth))preparedGpuSequence=visibleSequence;
                    if(preparedGpuSequence==visibleSequence&&gpuPresenter->present()){
                        lastGpuSequence=visibleSequence;if(lastPaint.time_since_epoch().count()!=0&&paintIntervals.size()<30000)paintIntervals.push_back(std::chrono::duration<double,std::milli>(stamp-lastPaint).count());lastPaint=stamp;++paintedFrames;
                        if(paintPerformance.is_open())paintPerformance<<hostNanoseconds(stamp)<<","<<visibleSequence<<","<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-stamp).count()<<","<<std::chrono::duration<double,std::milli>(stamp-visibleSubmitted).count()<<"\n";
                        if(displayPerformance.is_open()){DXGI_FRAME_STATISTICS stats{};const bool valid=gpuPresenter->statistics(stats);displayPerformance<<hostNanoseconds(stamp)<<","<<visibleSequence<<","<<visibleSourceFrame<<","<<gpuPresenter->lastPresent()<<","<<valid<<","<<stats.PresentCount<<","<<stats.PresentRefreshCount<<","<<stats.SyncRefreshCount<<","<<stats.SyncQPCTime.QuadPart<<"\n";}
                    }
                }
            }catch(const std::exception& e){std::cerr<<"GPU presentation fallback to GDI: "<<e.what()<<std::endl;gpuPresenter.reset();composer.setPresentationFilter(settings.smooth);InvalidateRect(window,nullptr,FALSE);}
            if(smoke&&frame.width&&unsigned(elapsed/5)>capturedBitmap){capturedBitmap=unsigned(elapsed/5);saveBitmap("native-frame-"+std::to_string(capturedBitmap*5)+(frameComplete?".bmp":".last-valid.bmp"));}
            // Yield the UI core; simulation/audio do not wait for this timeout.
            MsgWaitForMultipleObjects(0,nullptr,FALSE,1,QS_ALLINPUT);
        }
        running=false;gameThread.request_stop();gameThread.join();inputThread.request_stop();if(inputThread.joinable())inputThread.join();
        hostPhases.save("native-host-stalls.csv");gamePhases.save("native-game-stalls.csv");filePhases.save("native-input-file-stalls.csv");
        if(gameFailure)std::rethrow_exception(gameFailure);
        session->videoFrameReady={};
        if(!presentationError.empty())throw std::runtime_error(presentationError);
        const auto finalSequence=composer.submit(session->videoView(),session->gameNanoseconds());if(auto result=composer.waitFor(finalSequence,std::chrono::seconds(3))){if(!result->error.empty())throw std::runtime_error(result->error);frame=std::move(result->frame);}else throw std::runtime_error("Final native frame composition timed out");saveBitmap("native-frame.bmp");dumpVideo(session->videoView());{std::ofstream out("native-workram.bin",std::ios::binary);for(unsigned a=0;a<0x100000;a+=4){auto value=session->read(0x06000000+a,4);for(int shift=24;shift>=0;shift-=8)out.put(char(value>>shift));}}
        if(std::getenv("TECMO98_GPU_CAPTURE")){std::ofstream out("native-vdp1-source.bin",std::ios::binary);for(unsigned a=0;a<0x80000;a+=4){auto v=session->read(0x05c00000+a,4);for(int shift=24;shift>=0;shift-=8)out.put(char(v>>shift));}}
        std::sort(paintIntervals.begin(),paintIntervals.end());const auto paintP95=paintIntervals.empty()?0:paintIntervals[std::min(paintIntervals.size()-1,std::size_t(paintIntervals.size()*0.95))];
        if(smoke){
            saveWave();
            std::ofstream ram("native-soundram.bin",std::ios::binary),regs("native-scsp.bin",std::ios::binary);
            for(unsigned a=0;a<0x100000;++a)ram.put(char(session->read(0x05a00000+a,1)));
            for(unsigned a=0;a<0x1000;a+=2){const auto v=session->read(0x05b00000+a,2);regs.put(char(v>>8));regs.put(char(v));}
        }{std::ofstream out("native-frame-gates.txt");for(auto s:gates)out<<(s.slave?"S ":"M ")<<std::hex<<s.pc<<" "<<s.address<<" "<<s.size<<" "<<s.value<<"\n";}std::cout<<"{\"gpu_presents\":"<<(gpuPresenter?gpuPresenter->presentations():0)<<",\"source_video_frames\":"<<session->videoFrames()<<",\"seconds\":"<<elapsed<<",\"draw_lists\":"<<session->completedDrawLists()<<",\"main_pc\":"<<session->context.pc<<",\"nonzero_audio_frames\":"<<session->nonzeroAudioSamples()<<",\"main_instructions\":"<<session->context.executed<<",\"slave_instructions\":"<<session->slaveContext.executed<<",\"sound_instructions\":"<<session->soundContext.executed<<",\"sound_pc\":"<<session->soundContext.pc<<",\"zero_vector_angle_fixes\":"<<session->zeroVectorAngleFixes()<<",\"game_seconds\":"<<double(session->gameNanoseconds()-gameEpoch)/1e9<<",\"core_seconds\":"<<double(coreNs)/1e9<<",\"vdp1_seconds\":"<<double(session->hostWorkNanoseconds()[1])/1e9<<",\"dma_seconds\":"<<double(session->hostWorkNanoseconds()[2])/1e9<<",\"composition_ms_average\":"<<composer.averageMilliseconds()<<",\"composed_frames\":"<<composer.rendered()<<",\"presented_frames\":"<<presentedFrames<<",\"audio_underruns\":"<<(audio?audio->underruns():0)<<",\"audio_submitted_frames\":"<<(audio?audio->submitted():0)<<",\"audio_peak_queue_ms\":"<<(audio?audio->peakMilliseconds():0)<<",\"buffered_presentations\":"<<presenter.presentations()<<",\"presentation_allocations\":"<<presenter.allocations()<<",\"painted_frames\":"<<paintedFrames<<",\"paint_interval_p95_ms\":"<<paintP95<<",\"input_service_gap_max_ms\":"<<inputServiceMaxMs<<",\"input_packet_age_max_ms\":"<<launcherInput.maximumPacketAge()<<",\"input_packets\":"<<launcherInput.packets()<<",\"recovered_taps\":"<<launcherInput.recoveredTaps()<<",\"playable_validated\":false}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(frame.width)saveBitmap("native-frame.bmp");if(!recorded.empty())saveWave();return 3;}
}
