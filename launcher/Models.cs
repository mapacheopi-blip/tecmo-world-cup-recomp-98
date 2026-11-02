using System.IO;
using System.Text.Json;
namespace Tecmo98;
public sealed class Options {
 public string Language {get;set;}="en";
 public string RomSource {get;set;}=""; public string Backend {get;set;}="SDL";
 public bool Fullscreen {get;set;}=false;public string Resolution {get;set;}="1280x720";
 public string Aspect {get;set;}="Original";public string Filter {get;set;}="Nítido";
 public bool Vsync {get;set;}=true;public bool IntegerScale {get;set;}=false;
 public int Prescale {get;set;}=2;public bool Muted {get;set;}=false;public int Volume {get;set;}=0;public string Audio {get;set;}="wasapi";
 public int? VolumePercent {get;set;}
 public bool BackgroundInput {get;set;}=false;public bool AutoAssignFirst {get;set;}=true;
 public List<PlayerProfile> Players {get;set;}=Enumerable.Range(0,4).Select(i=>new PlayerProfile{Number=i+1,Keys=Defaults.Keys(i)}).ToList();
 public Dictionary<string,PadProfile> Pads {get;set;}=[];
}
public sealed class PlayerProfile {public int Number{get;set;}public string Device{get;set;}="";public string[] Keys{get;set;}=[];}
public sealed class PadProfile {public double Deadzone{get;set;}=.35;public string Directions{get;set;}="Auto";public string Family{get;set;}="Auto";public Binding?[] Bindings{get;set;}=new Binding?[10];}
public sealed record Binding(string Kind,int Index,int Sign=1,double Center=0) {
 public override string ToString()=>Kind switch {"button"=>L.F("Botón {0}",Index+1),"axis"=>L.F("Eje {0} {1}",Index+1,Sign<0?"−":"+"),"hat"=>L.F("Cruceta {0} · {1}",Index+1,new Dictionary<int,string>{{1,"↑"},{2,"→"},{4,"↓"},{8,"←"}}.GetValueOrDefault(Sign,"?")),_=>L.T("Sin asignar")};
}
public static class Defaults {
 public static readonly string[] Actions=["Arriba","Abajo","Izquierda","Derecha","Botón A","Botón B","Botón C","Botón D","Start","Crédito"];
 public static string[] Keys(int i)=>i switch {0=>["UP","DOWN","LEFT","RIGHT","Z","X","C","V","1","5"],1=>["W","S","A","D","F","G","H","R","2","6"],2=>["I","K","J","L","U","O","P","N","3","7"],_=>["8PAD","2PAD","4PAD","6PAD","0PAD","DELPAD","ENTERPAD","PLUSPAD","4","8"]};
}
public sealed class Store {
 public string Root{get;}
 public string NativeData=>Path.Combine(Root,"native");
 public string Documents{get;}
 public string Player=>Path.Combine(AppContext.BaseDirectory,"Player","tecmo98-native-player.exe");
 public string Roms=>Path.Combine(Root,"roms"); public string Runtime=>Path.Combine(Root,"runtime");
 public Options Value{get;private set;}=new();public string? Warning{get;private set;}
 public Store(string? root=null){Root=root??Path.Combine(AppContext.BaseDirectory,"DataNative");Documents=Path.Combine(AppContext.BaseDirectory,"Documentos");Directory.CreateDirectory(NativeData);Directory.CreateDirectory(Root);Directory.CreateDirectory(Roms);Directory.CreateDirectory(Runtime);
  var path=Path.Combine(Root,"settings.json");var legacy=Path.Combine(AppContext.BaseDirectory,"Data","settings.json");if(root==null&&!File.Exists(path)&&File.Exists(legacy))File.Copy(legacy,path);if(File.Exists(path))try {Value=JsonSerializer.Deserialize<Options>(File.ReadAllText(path))??new();Normalize();}catch(Exception ex){Warning="No se pudieron leer los ajustes. Se conservó una copia y se restauraron los valores iniciales.";File.Copy(path,path+".invalid-"+DateTime.Now.ToString("yyyyMMddHHmmss"),true);Value=new();System.Diagnostics.Debug.WriteLine(ex);}
 }
 void Normalize(){Value.Language=L.Normalize(Value.Language);if(Value.Players==null||Value.Players.Count!=4)Value.Players=Enumerable.Range(0,4).Select(i=>new PlayerProfile{Number=i+1,Keys=Defaults.Keys(i)}).ToList();for(int i=0;i<4;i++)if(Value.Players[i].Keys?.Length!=10)Value.Players[i].Keys=Defaults.Keys(i);foreach(var pl in Value.Players)for(int a=0;a<pl.Keys.Length;a++){pl.Keys[a]=System.Text.RegularExpressions.Regex.Replace(pl.Keys[a],"^NUMPAD([0-9])$","$1PAD").Replace("DEL_PAD","DELPAD").Replace("ENTER_PAD","ENTERPAD").Replace("ADD_PAD","PLUSPAD");}Value.Pads??=[];foreach(var p in Value.Pads.Values){p.Deadzone=double.IsFinite(p.Deadzone)?Math.Clamp(p.Deadzone,.15,.9):.35;if(p.Bindings?.Length!=10)p.Bindings=new Binding?[10];}Value.Volume=Math.Clamp(Value.Volume,-32,0);Value.VolumePercent=AudioOptions.Level(Value);Value.Prescale=Math.Clamp(Value.Prescale,1,4);}
 public void Save(){Normalize();var path=Path.Combine(Root,"settings.json");File.WriteAllText(path+".tmp",JsonSerializer.Serialize(Value,new JsonSerializerOptions{WriteIndented=true}));File.Move(path+".tmp",path,true);}
}


