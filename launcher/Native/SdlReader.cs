using System;
using System.Collections.Generic;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;
// SDL reads DirectInput with background input enabled. All values remain local.
public static class ArenaSDLControls {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern IntPtr LoadLibraryW(string path);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] [return:MarshalAs(UnmanagedType.I1)] static extern bool SDL_SetHint(string name,string value);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] [return:MarshalAs(UnmanagedType.I1)] static extern bool SDL_Init(uint flags);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern IntPtr SDL_GetError();
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern IntPtr SDL_GetJoysticks(out int count);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern void SDL_free(IntPtr ptr);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern void SDL_PumpEvents();
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] [return:MarshalAs(UnmanagedType.I1)] static extern bool SDL_PollEvent(out SdlInputEvent e);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern ulong SDL_GetTicksNS();
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern void SDL_UpdateJoysticks();
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern IntPtr SDL_OpenJoystick(uint id);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern void SDL_CloseJoystick(IntPtr joystick);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern IntPtr SDL_GetJoystickName(IntPtr joystick);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern ushort SDL_GetJoystickVendor(IntPtr joystick);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern ushort SDL_GetJoystickProduct(IntPtr joystick);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern int SDL_GetNumJoystickButtons(IntPtr joystick);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern int SDL_GetNumJoystickAxes(IntPtr joystick);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern int SDL_GetNumJoystickHats(IntPtr joystick);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] [return:MarshalAs(UnmanagedType.I1)] static extern bool SDL_GetJoystickButton(IntPtr joystick,int button);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern short SDL_GetJoystickAxis(IntPtr joystick,int axis);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern byte SDL_GetJoystickHat(IntPtr joystick,int hat);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] [return:MarshalAs(UnmanagedType.I1)] static extern bool SDL_IsGamepad(uint id);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern IntPtr SDL_OpenGamepad(uint id);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern void SDL_CloseGamepad(IntPtr gamepad);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] [return:MarshalAs(UnmanagedType.I1)] static extern bool SDL_GetGamepadButton(IntPtr gamepad,int button);
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern short SDL_GetGamepadAxis(IntPtr gamepad,int axis);
 static Dictionary<uint,IntPtr> sticks=new Dictionary<uint,IntPtr>();
 static Dictionary<uint,IntPtr> gamepads=new Dictionary<uint,IntPtr>();
 static DateTime scan=DateTime.MinValue;
 public static long QueuedDownEvents{get;private set;}
 public static long ExpiredEvents{get;private set;}
 public static long RecoveredButtons{get;private set;}
 public static SdlButtonEvents DrainButtonEvents(){var edges=new SdlButtonEvents();while(SDL_PollEvent(out var e))edges.Add(e,SDL_GetTicksNS());QueuedDownEvents+=edges.DownEvents;ExpiredEvents+=edges.ExpiredEvents;return edges;}
 static string Q(string? s){return "\""+(s??"").Replace("\\","\\\\").Replace("\"","\\\"").Replace("\r","").Replace("\n","")+"\"";}
 static string Number(double n){return n.ToString("0.0000",CultureInfo.InvariantCulture);}
 public static void Init(string dll){
  if(LoadLibraryW(dll)==IntPtr.Zero)throw new Exception("No se pudo cargar SDL3");
  SDL_SetHint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS","1");SDL_SetHint("SDL_XINPUT_ENABLED","1");SDL_SetHint("SDL_JOYSTICK_DIRECTINPUT","1");SDL_SetHint("SDL_JOYSTICK_RAWINPUT","1");SDL_SetHint("SDL_JOYSTICK_RAWINPUT_CORRELATE_XINPUT","1");SDL_SetHint("SDL_JOYSTICK_HIDAPI","1");
  if(!SDL_Init(0x2200))throw new Exception(Marshal.PtrToStringAnsi(SDL_GetError()));
  scan=DateTime.MinValue;QueuedDownEvents=ExpiredEvents=RecoveredButtons=0;
 }
 public static void Dispose(){foreach(var g in gamepads.Values)SDL_CloseGamepad(g);foreach(var j in sticks.Values)SDL_CloseJoystick(j);gamepads.Clear();sticks.Clear();SDL_Quit();}
 [DllImport("SDL3",CallingConvention=CallingConvention.Cdecl)] static extern void SDL_Quit();
 public static string Sample(){
  SDL_PumpEvents();
  if(DateTime.UtcNow>=scan){int count;IntPtr list=SDL_GetJoysticks(out count);var present=new HashSet<uint>();for(int i=0;i<count;i++){uint id=unchecked((uint)Marshal.ReadInt32(list,i*4));present.Add(id);if(!sticks.ContainsKey(id)){IntPtr stick=SDL_OpenJoystick(id);if(stick!=IntPtr.Zero){sticks[id]=stick;if(SDL_IsGamepad(id)){IntPtr gamepad=SDL_OpenGamepad(id);if(gamepad!=IntPtr.Zero)gamepads[id]=gamepad;}}}}SDL_free(list);
   var removed=new List<uint>();foreach(var p in sticks)if(!present.Contains(p.Key))removed.Add(p.Key);foreach(uint id in removed){if(gamepads.ContainsKey(id)){SDL_CloseGamepad(gamepads[id]);gamepads.Remove(id);}SDL_CloseJoystick(sticks[id]);sticks.Remove(id);}scan=DateTime.UtcNow.AddSeconds(1);
  }
  SDL_UpdateJoysticks();var edges=DrainButtonEvents();
  var json=new StringBuilder("[");bool comma=false;
  foreach(var entry in sticks){uint id=entry.Key;IntPtr j=entry.Value;int buttons=Math.Min(64,SDL_GetNumJoystickButtons(j)),axes=Math.Max(0,SDL_GetNumJoystickAxes(j)),hats=Math.Max(0,SDL_GetNumJoystickHats(j));ulong mask=0;for(int b=0;b<buttons;b++)if(SDL_GetJoystickButton(j,b))mask|=1UL<<b;
   if(comma)json.Append(',');comma=true;json.Append("{\"kind\":\"sdl\",\"index\":").Append(id).Append(",\"name\":").Append(Q(Marshal.PtrToStringAnsi(SDL_GetJoystickName(j)))).Append(",\"vendorId\":").Append(Q(SDL_GetJoystickVendor(j).ToString("x4"))).Append(",\"productId\":").Append(Q(SDL_GetJoystickProduct(j).ToString("x4"))).Append(",\"buttonCount\":").Append(buttons).Append(",\"buttons\":").Append(mask).Append(",\"pov\":65535,\"axes\":[");
   for(int a=0;a<axes;a++){if(a>0)json.Append(',');json.Append(Number(SDL_GetJoystickAxis(j,a)/32768.0));}json.Append("],\"hats\":[");for(int h=0;h<hats;h++){if(h>0)json.Append(',');json.Append(SDL_GetJoystickHat(j,h));}json.Append(']');
   json.Append(",\"pressedButtons\":").Append(edges.Raw(id));
   if(!gamepads.ContainsKey(id))RecoveredButtons+=System.Numerics.BitOperations.PopCount(edges.Raw(id)&~mask);
   if(gamepads.ContainsKey(id)){IntPtr g=gamepads[id];var order=new int[]{0,1,2,3,9,10,-1,-2,4,6,7,8,11,12,13,14,5};json.Append(",\"standardButtons\":[");for(int b=0;b<order.Length;b++){if(b>0)json.Append(',');bool on=order[b]>=0?SDL_GetGamepadButton(g,order[b]):SDL_GetGamepadAxis(g,order[b]==-1?4:5)>14745;json.Append(on?'1':'0');if(!on&&(edges.Standard(id)&(1UL<<b))!=0)RecoveredButtons++;}json.Append("],\"standardAxes\":[");for(int a=0;a<4;a++){if(a>0)json.Append(',');json.Append(Number(SDL_GetGamepadAxis(g,a)/32768.0));}json.Append(']');}
   if(gamepads.ContainsKey(id)){ulong down=edges.Standard(id);json.Append(",\"standardPressedButtons\":[");for(int b=0;b<17;b++){if(b>0)json.Append(',');json.Append((down&(1UL<<b))!=0?'1':'0');}json.Append(']');}
   json.Append('}');
  }
  return json.Append(']').ToString();
 }
}


