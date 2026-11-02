using System.IO;
using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Serialization;
namespace Tecmo98;
public sealed class NativeSample {
 public string Kind{get;set;}="sdl";public uint Index{get;set;} public string Name{get;set;}="Mando";public string VendorId{get;set;}="";public string ProductId{get;set;}="";public string InstanceGuid{get;set;}="";
 public int ButtonCount{get;set;}public ulong Buttons{get;set;}public ulong PressedButtons{get;set;}public int[]? StandardPressedButtons{get;set;}public double[] Axes{get;set;}=[];public int[] Hats{get;set;}=[];public int[]? StandardButtons{get;set;}public double[]? StandardAxes{get;set;}public bool Acquired{get;set;}=true;
}
public sealed class PadState {
 public string Key{get;set;}="";public string SessionId{get;set;}="";public string Name{get;set;}="";public string Family{get;set;}="generic";public string Driver{get;set;}="";public bool Standard{get;set;}
 public bool[] Buttons{get;set;}=[];public bool[] ButtonPresses{get;set;}=[];public double[] Axes{get;set;}=[];public int[] Hats{get;set;}=[];public NativeSample Raw{get;set;}=new();
 public override string ToString()=>Name;
}
public static class PadLogic {
 public static readonly JsonSerializerOptions Json=new(){PropertyNameCaseInsensitive=true};
 public static List<PadState> Parse(string json){var result=new List<PadState>();var copies=new Dictionary<string,int>();
  foreach(var s in JsonSerializer.Deserialize<List<NativeSample>>(json,Json)??[]){if(!s.Acquired)continue;bool standard=s.StandardButtons!=null;var key=$"{s.Kind}|{s.VendorId}:{s.ProductId}|{s.Name}|{(standard?"standard":"raw")}|{s.ButtonCount}|{s.Axes.Length}|{s.Hats.Length}";int n=copies.GetValueOrDefault(key);copies[key]=n+1;key+="|"+n;
   string family=s.VendorId=="057e"?"nintendo":s.VendorId=="054c"||s.VendorId=="0f0d"||s.VendorId=="0c12"||System.Text.RegularExpressions.Regex.IsMatch(s.Name,"Dual|PlayStation|HORI",System.Text.RegularExpressions.RegexOptions.IgnoreCase)?"playstation":s.VendorId=="045e"||s.Name.Contains("Xbox",StringComparison.OrdinalIgnoreCase)?"xbox":"generic";
   result.Add(new(){Key=key,SessionId=s.Kind+":"+s.Index,Name=s.Name+(n>0?$" · unidad {n+1}":""),Family=family,Driver=s.Kind=="sdl"?"SDL · HID / XInput / DirectInput":"DirectInput compartido",Standard=standard,Buttons=standard?s.StandardButtons!.Select(x=>x!=0).ToArray():Enumerable.Range(0,Math.Clamp(s.ButtonCount,0,64)).Select(i=>(s.Buttons&(1UL<<i))!=0).ToArray(),Axes=standard?s.StandardAxes??[]:s.Axes,Hats=s.Hats,Raw=s});
   var added=result[^1];added.ButtonPresses=standard?s.StandardPressedButtons?.Select(x=>x!=0).ToArray()??[]:Enumerable.Range(0,Math.Clamp(s.ButtonCount,0,64)).Select(i=>(s.PressedButtons&(1UL<<i))!=0).ToArray();
  }return result;
 }
 public static PadProfile Default(PadState p){var b=new Binding?[10];if(p.Standard){for(int i=0;i<4;i++)b[i]=new("button",12+i);for(int i=0;i<4;i++)b[i+4]=new("button",i);b[8]=new("button",9);b[9]=new("button",8);}else{if(p.Hats.Length>0){int[] dir=[1,4,8,2];for(int i=0;i<4;i++)b[i]=new("hat",0,dir[i]);}else if(p.Axes.Length>=2){b[0]=new("axis",1,-1);b[1]=new("axis",1,1);b[2]=new("axis",0,-1);b[3]=new("axis",0,1);}for(int i=0;i<4&&i<p.Buttons.Length;i++)b[i+4]=new("button",i);if(p.Buttons.Length>9){b[8]=new("button",9);b[9]=new("button",8);}}return new(){Bindings=b};}
 public static bool Pressed(PadState p,Binding? b,double dz){if(b==null||b.Index<0||b.Index>=128)return false;return b.Kind switch {"button"=>b.Index<p.Buttons.Length&&p.Buttons[b.Index],"axis"=>b.Index<p.Axes.Length&&double.IsFinite(p.Axes[b.Index])&&(p.Axes[b.Index]-b.Center)*b.Sign>dz,"hat"=>b.Index<p.Hats.Length&&(p.Hats[b.Index]&b.Sign)!=0,_=>false};}
 public static int[] Read(PadState p,PadProfile c){int[] v=c.Bindings.Select((b,a)=>Pressed(p,b,c.Deadzone)||a>=4&&b?.Kind=="button"&&b.Index>=0&&b.Index<p.ButtonPresses.Length&&p.ButtonPresses[b.Index]?1:0).ToArray();if(v.Length!=10)return new int[10];
  void Pair(int x,int y){if(x>=p.Axes.Length||y>=p.Axes.Length)return;if(p.Axes[x]<-c.Deadzone)v[2]=1;if(p.Axes[x]>c.Deadzone)v[3]=1;if(p.Axes[y]<-c.Deadzone)v[0]=1;if(p.Axes[y]>c.Deadzone)v[1]=1;}
  if(c.Directions=="Auto"||c.Directions=="Izquierdo")Pair(0,1);if(c.Directions=="Derecho")Pair(p.Standard?2:3,p.Standard?3:4);
  if(c.Directions=="Auto"&&p.Hats.Length>0){int h=p.Hats[0];if((h&1)!=0)v[0]=1;if((h&4)!=0)v[1]=1;if((h&8)!=0)v[2]=1;if((h&2)!=0)v[3]=1;}
  if(v[0]!=0&&v[1]!=0)v[0]=v[1]=0;if(v[2]!=0&&v[3]!=0)v[2]=v[3]=0;return v;
 }
 public static bool Neutral(PadState p)=>!p.Buttons.Any(x=>x)&&p.Hats.All(x=>x==0)&&p.Axes.Select((x,i)=>Math.Abs(x)<.25||i>=2&&Math.Abs(x+1)<.05).All(x=>x);
 public static Binding? Capture(PadState before,PadState now){for(int i=0;i<now.Buttons.Length;i++)if(i<now.ButtonPresses.Length&&now.ButtonPresses[i]||now.Buttons[i]&&(i>=before.Buttons.Length||!before.Buttons[i]))return new("button",i);
  for(int i=0;i<now.Hats.Length;i++){int h=now.Hats[i],old=i<before.Hats.Length?before.Hats[i]:0;if(new[]{1,2,4,8}.Contains(h)&&h!=old)return new("hat",i,h);}
  for(int i=0;i<now.Axes.Length;i++){double b=i<before.Axes.Length?before.Axes[i]:0,n=now.Axes[i];if(Math.Abs(b)<=.25&&Math.Abs(n)>.65)return new("axis",i,Math.Sign(n),0);if(i>=2&&b<-.95&&n-b>.65)return new("axis",i,1,-1);}return null;
 }
 public static string Label(Binding? b,PadState? p,string family){if(b==null)return "Asignar";if(b.Kind!="button"||p?.Standard!=true)return b.ToString();string f=family=="Auto"?p.Family:family.ToLowerInvariant();string[] names=f switch{"playstation"=>["×","○","□","△","L1","R1","L2","R2","Share","Options","L3","R3","↑","↓","←","→","PS"],"xbox"=>["A","B","X","Y","LB","RB","LT","RT","View","Menu","LS","RS","↑","↓","←","→","Guide"],"nintendo"=>["B","A","Y","X","L","R","ZL","ZR","−","+","LS","RS","↑","↓","←","→","Home"],_=>[]};return b.Index<names.Length?names[b.Index]:b.ToString();}
}
public sealed class InputService : IDisposable {
 bool initialized;string backend="";Process? helper;string latest="[]";long received;readonly object gate=new();public string? Error{get;private set;}
 public List<PadState> Poll(string requested){try{if(backend!=requested){Stop();backend=requested;Error=null;if(requested=="SDL"){ArenaSDLControls.Init(Path.Combine(AppContext.BaseDirectory,"SDL3.dll"));initialized=true;}else{var path=Path.Combine(AppContext.BaseDirectory,"InputBridge","InputBridge.exe");if(!File.Exists(path))throw new FileNotFoundException("Falta el lector DirectInput. Usa SDL o recompila el lector.");helper=Process.Start(new ProcessStartInfo(path){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true});helper!.OutputDataReceived+=(_,e)=>{if(e.Data?.StartsWith('[')==true)lock(gate){latest=e.Data;received=Environment.TickCount64;}};helper.ErrorDataReceived+=(_,e)=>{if(e.Data!=null)Error=e.Data;};helper.BeginOutputReadLine();helper.BeginErrorReadLine();}}
   if(requested=="SDL")return PadLogic.Parse(ArenaSDLControls.Sample());lock(gate)return Environment.TickCount64-received<450?PadLogic.Parse(latest):[];
  }catch(Exception ex){Error=ex.Message;return [];}}
 void Stop(){if(initialized){ArenaSDLControls.Dispose();initialized=false;}if(helper!=null){try{if(!helper.HasExited)helper.Kill(true);}catch{}helper.Dispose();helper=null;}lock(gate){latest="[]";received=0;}backend="";}
 public void Dispose()=>Stop();
}

