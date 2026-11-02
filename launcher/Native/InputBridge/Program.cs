using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text.Json;
using Vortice.DirectInput;

// Read only joystick devices. A hidden top-level window owns Background | NonExclusive acquisition.
// No keyboard/mouse capture, driver installation or changes to system configuration.
internal static class Program
{
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern nint CreateWindowExW(uint ex,string cls,string name,uint style,int x,int y,int w,int h,nint parent,nint menu,nint instance,nint param);
 [DllImport("user32.dll")] static extern bool DestroyWindow(nint hwnd);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern nint GetModuleHandleW(string? name);
 [StructLayout(LayoutKind.Sequential)] struct Msg{public nint hwnd;public uint message;public nuint wp;public nint lp;public uint time;public int x,y;public uint extra;}
 [DllImport("user32.dll")] static extern bool PeekMessageW(out Msg message,nint hwnd,uint min,uint max,uint remove);
 [DllImport("user32.dll")] static extern bool TranslateMessage(ref Msg message);
 [DllImport("user32.dll")] static extern nint DispatchMessageW(ref Msg message);
 sealed record Device(uint Index,DeviceInstance Info,IDirectInputDevice8 Input);
 static readonly JsonSerializerOptions Json=new(){PropertyNamingPolicy=JsonNamingPolicy.CamelCase};
 static double Axis(int n)=>Math.Clamp(n/32768.0,-1,1);
 static int Hat(int pov){if(pov<0||pov>=36000)return 0;return new[]{1,3,2,6,4,12,8,9}[(int)Math.Round(pov/4500.0)%8];}
 static int Main(string[] args)
 {
  nint window=CreateWindowExW(0,"STATIC","Puño Arena Input",0,0,0,1,1,0,0,GetModuleHandleW(null),0);
  if(window==0)throw new InvalidOperationException("No se pudo crear la ventana de adquisición.");
  using var directInput=DInput.DirectInput8Create();
  var devices=new Dictionary<Guid,Device>();uint nextId=1;var watch=Stopwatch.StartNew();long scan=-1000,lastOutput=-1000;string previous="";
  int seconds=args.Length>0&&int.TryParse(args[0],out int parsed)?parsed:0;
  try{
   while(seconds<=0||watch.Elapsed.TotalSeconds<seconds){
    while(PeekMessageW(out var message,0,0,0,1)){TranslateMessage(ref message);DispatchMessageW(ref message);}
    if(watch.ElapsedMilliseconds-scan>=1000){
     var attached=directInput.GetDevices(DeviceClass.GameControl,DeviceEnumerationFlags.AttachedOnly);var ids=attached.Select(d=>d.InstanceGuid).ToHashSet();
     foreach(var id in devices.Keys.Where(id=>!ids.Contains(id)).ToArray()){devices[id].Input.Unacquire();devices[id].Input.Dispose();devices.Remove(id);}
     foreach(var info in attached){if(devices.ContainsKey(info.InstanceGuid))continue;IDirectInputDevice8? input=null;
      try{input=directInput.CreateDevice(info.InstanceGuid);input.SetDataFormat<RawJoystickState>().CheckError();input.SetCooperativeLevel(window,CooperativeLevel.Background|CooperativeLevel.NonExclusive).CheckError();input.Properties.Range=new InputRange(-32768,32767);input.Acquire();devices[info.InstanceGuid]=new Device(nextId++,info,input);}
      catch(Exception e){input?.Dispose();Console.Error.WriteLine("Acquire setup: "+e.Message);}
     }scan=watch.ElapsedMilliseconds;
    }
    var samples=new List<object>();
    foreach(var device in devices.Values){
     try{var poll=device.Input.Poll();if(poll.Failure){var acquired=device.Input.Acquire();if(acquired.Failure)continue;if(device.Input.Poll().Failure)continue;}
      var state=device.Input.GetCurrentJoystickState();uint buttons=0;for(int b=0;b<32;b++)if(state.Buttons[b])buttons|=1u<<b;
      byte[] product=device.Info.ProductGuid.ToByteArray();int vendor=BitConverter.ToUInt16(product,0),pid=BitConverter.ToUInt16(product,2);
      samples.Add(new{kind="directinput",index=device.Index,name=device.Info.ProductName,vendorId=vendor.ToString("x4"),productId=pid.ToString("x4"),buttonCount=Math.Min(32,device.Input.Capabilities.ButtonCount),buttons,pov=state.PointOfViewControllers[0],hats=state.PointOfViewControllers.Select(Hat).ToArray(),axes=new[]{Axis(state.X),Axis(state.Y),Axis(state.Z),Axis(state.RotationX),Axis(state.RotationY),Axis(state.RotationZ)},acquired=true});
     }catch(Exception e){device.Input.Unacquire();device.Input.Acquire();Console.Error.WriteLine("Poll: "+e.Message);}
    }
    string line=JsonSerializer.Serialize(samples,Json);if(line!=previous||watch.ElapsedMilliseconds-lastOutput>=200){Console.WriteLine(line);previous=line;lastOutput=watch.ElapsedMilliseconds;}
    Thread.Sleep(4);
   }
  }finally{foreach(var d in devices.Values){d.Input.Unacquire();d.Input.Dispose();}DestroyWindow(window);}
  return 0;
 }
}
