using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Windows.Threading;
namespace Tecmo98;

// Real SDL virtual devices and event queue; no OS-wide injected input.
public static class SdlInputRegression {
 [StructLayout(LayoutKind.Explicit,Size=136)] struct VirtualDesc {
  [FieldOffset(0)] public uint Version;
  [FieldOffset(4)] public ushort Type;
  [FieldOffset(8)] public ushort Vendor;
  [FieldOffset(10)] public ushort Product;
  [FieldOffset(12)] public ushort Axes;
  [FieldOffset(14)] public ushort Buttons;
  [FieldOffset(18)] public ushort Hats;
  [FieldOffset(28)] public uint ButtonMask;
  [FieldOffset(32)] public uint AxisMask;
  [FieldOffset(40)] public nint Name;
 }
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern uint SDL_AttachVirtualJoystick(ref VirtualDesc desc);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern nint SDL_OpenJoystick(uint id);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern void SDL_CloseJoystick(nint joystick);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] [return:MarshalAs(UnmanagedType.I1)] static extern bool SDL_DetachVirtualJoystick(uint id);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] [return:MarshalAs(UnmanagedType.I1)] static extern bool SDL_SetJoystickVirtualButton(nint joystick,int button,[MarshalAs(UnmanagedType.I1)] bool down);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern void SDL_UpdateJoysticks();
 static void Check(bool value,string message){if(!value)throw new Exception(message);}
 static (uint,nint) Attach(bool standard){var name=Marshal.StringToCoTaskMemUTF8(standard?"TECMO regression gamepad":"TECMO regression raw");try{
  var desc=new VirtualDesc{Version=136,Type=(ushort)(standard?1:0),Vendor=0xf098,Product=(ushort)(standard?2:1),Axes=6,Buttons=17,Hats=1,ButtonMask=0x1ffff,AxisMask=0x3f,Name=name};
  uint id=SDL_AttachVirtualJoystick(ref desc);Check(id!=0,"Attach virtual device");nint handle=SDL_OpenJoystick(id);Check(handle!=0,"Open virtual device");return(id,handle);
 }finally{Marshal.FreeCoTaskMem(name);}}
 static void Tap(nint handle,int button){Check(SDL_SetJoystickVirtualButton(handle,button,true),"Virtual button down");SDL_UpdateJoysticks();Check(SDL_SetJoystickVirtualButton(handle,button,false),"Virtual button up");SDL_UpdateJoysticks();}
 static PadState Find(List<PadState> pads,uint id)=>pads.Single(p=>p.Raw.Index==id);
 public static void Run(){
  using var inputs=new InputService();inputs.Poll("SDL");
  var(rawId,raw)=Attach(false);var(stdId,std)=Attach(true);
  try{
   System.Threading.Thread.Sleep(1050);var pads=inputs.Poll("SDL");var r=Find(pads,rawId);var s=Find(pads,stdId);Check(!r.Standard&&s.Standard,"Virtual raw and standard modes");
   int recovered=0;for(int i=0;i<480;i++){
    int b=i%4;Tap(raw,b);Tap(std,b);
    pads=inputs.Poll("SDL");r=Find(pads,rawId);s=Find(pads,stdId);
    foreach(var p in new[]{r,s}){Check(!p.Buttons[b],"Getter must already be released (old path would miss tap)");Check(PadLogic.Read(p,PadLogic.Default(p))[4+b]==1,"Short tap lost before encoder");recovered++;}
    pads=inputs.Poll("SDL");Check(PadLogic.Read(Find(pads,rawId),PadLogic.Default(r)).All(x=>x==0),"Raw event replayed");Check(PadLogic.Read(Find(pads,stdId),PadLogic.Default(s)).All(x=>x==0),"Standard event replayed");
   }
   Tap(std,0);System.Threading.Thread.Sleep(120);s=Find(inputs.Poll("SDL"),stdId);Check(PadLogic.Read(s,PadLogic.Default(s))[4]==1,"Short tap lost across 120 ms UI stall");
   Tap(std,0);System.Threading.Thread.Sleep(280);s=Find(inputs.Poll("SDL"),stdId);Check(PadLogic.Read(s,PadLogic.Default(s))[4]==0,"Expired event replayed");
   var c=PadLogic.Default(s);s.ButtonPresses=new bool[17];s.ButtonPresses[12]=true;Check(PadLogic.Read(s,c)[0]==0,"Queued D-pad must not prolong directions");
   // Held state still works without events; releasing returns neutral on next sample.
   Check(SDL_SetJoystickVirtualButton(std,1,true),"Held press");s=Find(inputs.Poll("SDL"),stdId);Check(PadLogic.Read(s,c)[5]==1,"Held button lost");s=Find(inputs.Poll("SDL"),stdId);Check(PadLogic.Read(s,c)[5]==1,"Held button depends on event");Check(SDL_SetJoystickVirtualButton(std,1,false),"Release held");s=Find(inputs.Poll("SDL"),stdId);Check(PadLogic.Read(s,c)[5]==0,"Held button stuck");
   SDL_CloseJoystick(raw);raw=0;Check(SDL_DetachVirtualJoystick(rawId),"Detach raw");SDL_CloseJoystick(std);std=0;Check(SDL_DetachVirtualJoystick(stdId),"Detach standard");
   System.Threading.Thread.Sleep(1050);Check(inputs.Poll("SDL").All(p=>p.Raw.Index!=rawId&&p.Raw.Index!=stdId),"Detached virtual device retained");
   string dir=Path.Combine(AppContext.BaseDirectory,"verification");Directory.CreateDirectory(dir);File.WriteAllText(Path.Combine(dir,"sdl-input-regression.json"),JsonSerializer.Serialize(new{passed=true,source="SDL virtual gamepad and raw joystick; not physical controller",short_taps_recovered=recovered,old_getter_state_released=true,stall_120_ms=true,expired_280_ms_rejected=true,held_release=true,directions_not_prolonged=true,disconnect=true,events=ArenaSDLControls.QueuedDownEvents,expired=ArenaSDLControls.ExpiredEvents},new JsonSerializerOptions{WriteIndented=true}));
  }finally{if(raw!=0){SDL_CloseJoystick(raw);SDL_DetachVirtualJoystick(rawId);}if(std!=0){SDL_CloseJoystick(std);SDL_DetachVirtualJoystick(stdId);}}
 }
 public static async Task Match(bool quick=false,bool rejectQuitPacket=false,bool sharp=false,bool longer=false){
  string dir=Path.Combine(AppContext.BaseDirectory,"verification");var store=new Store(Path.Combine(dir,"input-match"));
  RomService.ImportNative(Path.GetFullPath(Path.Combine(AppContext.BaseDirectory,"..","original","twcup98.zip")),store);
  store.Value.BackgroundInput=true;store.Value.Resolution="960x720";store.Value.Filter=sharp?"Nítido":"Pixelart 2x";store.Value.VolumePercent=4;
  using var inputs=new InputService();inputs.Poll("SDL");var(id,handle)=Attach(true);
  var game=new GameService(store);var timer=new DispatcherTimer(DispatcherPriority.Input){Interval=TimeSpan.FromMilliseconds(8)};
  try{
   await Task.Delay(1100);var pad=Find(inputs.Poll("SDL"),id);var profile=PadLogic.Default(pad);game.Start();
   var clock=Stopwatch.StartNew();int sent=0,observed=0,lastAction=-1;bool credit=false,start=false,confirm=false,stalled=false;Exception? failure=null;
   timer.Tick+=(_,_)=>{try{
    double t=clock.Elapsed.TotalSeconds;int b=-1;
    if(t>=2&&!credit){credit=true;b=4;} // SDL BACK -> normalized index 8 -> credit
    else if(t>=4&&!start){start=true;b=6;} // SDL START -> normalized index 9
    else if(t>=8&&!confirm){confirm=true;b=0;}
    else if(t>=18){int n=(int)((t-18)*4);if(n!=lastAction){lastAction=n;b=n%3;}}
    if(b>=0){Tap(handle,b);sent++;if(t>=25&&!stalled){stalled=true;System.Threading.Thread.Sleep(120);}}
    var p=Find(inputs.Poll("SDL"),id);var v=PadLogic.Read(p,profile);int mask=0;for(int a=0;a<10;a++)if(v[a]!=0)mask|=1<<a;
    if(b>=0){int a=b==4?9:b==6?8:4+b;Check(v[a]!=0,"Match lost an SDL tap");observed++;}
    if(t>=2&&t<2.3)mask|=512;if(t>=4&&t<4.3)mask|=256;
    if(t>=18)mask|=8;
    game.WriteState([mask,0,0,0]);game.NoteInput(game.GameFocused,false,1,inputs.Error);
   }catch(Exception ex){failure=ex;timer.Stop();}};
   timer.Start();await Task.Delay(quick?28000:longer?135000:65000);timer.Stop();if(failure!=null)throw failure;Check(sent==observed&&sent>(quick?20:180),"Sustained short tap count");
   // Let the last queued press reach the game's next input read before closing
   // its window. Otherwise the regression itself can cut off its final tap.
   game.WriteState(new int[4]);await Task.Delay(100);
   if(rejectQuitPacket){using var locked=new FileStream(Path.Combine(game.LastSession!,"input.txt"),FileMode.Open,FileAccess.Read,FileShare.Read);game.Stop();}
   else game.Stop();
   for(int i=0;i<500&&game.Running;i++)await Task.Delay(20);Check(!game.Running&&game.ExitCode==0,"Native match test did not exit normally");game.FinishInputLog();
   string log=File.ReadAllText(Path.Combine(game.LastSession!,"stdout.log"));Check(log.Contains("\"draw_lists\":"),"Missing native summary");File.WriteAllText(Path.Combine(dir,"sdl-input-match.json"),JsonSerializer.Serialize(new{passed=true,seconds=clock.Elapsed.TotalSeconds,source="SDL virtual gamepad through production sampler, PadLogic, encoder and native game; not physical controller",sent,observed,ui_stall_120_ms=stalled,filter=store.Value.Filter,quit_packet_deliberately_locked=rejectQuitPacket,session=game.LastSession},new JsonSerializerOptions{WriteIndented=true}));
  }finally{timer.Stop();game.Close();SDL_CloseJoystick(handle);SDL_DetachVirtualJoystick(id);}
 }
}
