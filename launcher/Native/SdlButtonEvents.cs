using System.Runtime.InteropServices;

// SDL3 ABI: SDL_Event is 128 bytes; joystick/gamepad button fields share offsets.
// Source: libsdl-org/SDL release-3.2.28/include/SDL3/SDL_events.h.
[StructLayout(LayoutKind.Explicit, Size=128)]
public struct SdlInputEvent {
 [FieldOffset(0)] public uint Type;
 [FieldOffset(8)] public ulong Timestamp;
 [FieldOffset(16)] public uint Which;
 [FieldOffset(20)] public byte Button;
 [FieldOffset(21)] public byte Down;
 [FieldOffset(24)] public short AxisValue;
}

public sealed class SdlButtonEvents {
 readonly Dictionary<uint,ulong> raw=new(),standard=new();
 static readonly int[] order=[0,1,2,3,9,10,-1,-2,4,6,7,8,11,12,13,14,5];
 public long DownEvents {get;private set;}
 public long ExpiredEvents {get;private set;}
 public void Add(SdlInputEvent e,ulong now){
  if(e.Type!=0x603&&e.Type!=0x651&&e.Type!=0x650)return;
  // Do not replay input queued before a lengthy stall / suspend.
  if(e.Timestamp>now||now-e.Timestamp>250_000_000){ExpiredEvents++;return;}
  if(e.Type==0x603&&e.Button<64){raw[e.Which]=Raw(e.Which)|(1UL<<e.Button);DownEvents++;}
  if(e.Type==0x651){int b=Array.IndexOf(order,(int)e.Button);if(b>=0){standard[e.Which]=Standard(e.Which)|(1UL<<b);DownEvents++;}}
  // Standard L2/R2 are axes, exposed as buttons 6/7 by the existing profiles.
  if(e.Type==0x650&&e.Button is 4 or 5&&e.AxisValue>14745){standard[e.Which]=Standard(e.Which)|(1UL<<(e.Button==4?6:7));DownEvents++;}
 }
 public ulong Raw(uint id)=>raw.GetValueOrDefault(id);
 public ulong Standard(uint id)=>standard.GetValueOrDefault(id);
}
