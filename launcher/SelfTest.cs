using System.IO;
using System.Text.Json;
namespace Tecmo98;
public static class SelfTest {
 static void Check(bool ok,string message){if(!ok)throw new Exception(message);}
 public static void Run(){var passed=new List<string>();
  string oldLanguage=L.Language;
  foreach(string language in new[]{"en","es"}){
   L.Language=language;
   foreach(var key in L.English.Keys)Check(L.F(key,0,0,0,0,0,0,0,0,0,0).Length>0,"Empty/invalid localized resource: "+key);
  }
  L.Language=oldLanguage;passed.Add("English/Spanish catalog: all format resources render in both languages");

  var raw=PadLogic.Parse("[{\"kind\":\"sdl\",\"index\":1,\"name\":\"Brook Fighting Board\",\"vendorId\":\"0c12\",\"productId\":\"0ef8\",\"buttonCount\":11,\"buttons\":1,\"axes\":[0,0,0,0,0,0],\"hats\":[3]}]").Single();
  var config=PadLogic.Default(raw);var v=PadLogic.Read(raw,config);Check(v[0]==1&&v[3]==1&&v[4]==1,"Brook diagonal and button");raw.Hats=[0];raw.Buttons=new bool[11];Check(PadLogic.Read(raw,config).All(x=>x==0),"HAT center must be neutral");passed.Add("Brook raw: diagonal, center and independent buttons");
  raw.Axes=[-1,0,0,0,0,0];raw.Hats=[2];v=PadLogic.Read(raw,config);Check(v[2]==0&&v[3]==0,"SOCD neutral");passed.Add("Opposite directions cancel");
  var standard=PadLogic.Parse("[{\"index\":2,\"vendorId\":\"054c\",\"productId\":\"0ce6\",\"name\":\"DualSense\",\"buttonCount\":15,\"buttons\":0,\"axes\":[0,0,0,0],\"hats\":[0],\"standardButtons\":[1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],\"standardAxes\":[0,0,0,0]}]").Single();Check(standard.Standard&&PadLogic.Read(standard,PadLogic.Default(standard))[4]==1,"Standardized face index");Check(PadLogic.Label(new("button",0),standard,"Auto")=="×","PS symbols independent of raw indices");passed.Add("Standard gamepad mapping and PlayStation labels");
  var neutral=new PadState{Buttons=new bool[12],Axes=[0,0,-1],Hats=[0]};var pressed=new PadState{Buttons=new bool[12],Axes=[0,0,.1],Hats=[0]};Check(PadLogic.Capture(neutral,pressed)==new Binding("axis",2,1,-1),"Resting-negative trigger capture");pressed.Hats=[8];Check(PadLogic.Capture(neutral,pressed)==new Binding("hat",0,8),"HAT capture before axes");Check(PadLogic.Neutral(neutral),"Negative trigger neutral");passed.Add("Capture: neutral gating, HAT and negative-centered trigger");
  Check(PadLogic.Parse("[]").Count==0,"Disconnect empties sample");Check(PadLogic.Parse("[{\"acquired\":false,\"kind\":\"directinput\"}]").Count==0,"Reject unacquired DirectInput state");passed.Add("Disconnect / failed acquisition produce no inputs");
  var changed=PadLogic.Parse("[{\"name\":\"Brook Fighting Board\",\"vendorId\":\"0c12\",\"productId\":\"0ef8\",\"buttonCount\":10,\"axes\":[0,0,0,0,0],\"hats\":[0]}]").Single();Check(raw.Key!=changed.Key,"Firmware layouts need different profiles");passed.Add("Profiles separate raw layout and driver modes");
  var o=new Options{Resolution="evil -speed 9",Volume=400,Filter="Suave",Fullscreen=true};var settings=GameService.BuildSettings(o);Check(settings=="TECMO98_NATIVE_V2 1280 720 1 0 1 100 0 0\n","Settings whitelist and bounds");passed.Add("Native launch settings: bounds, fullscreen, smoothing, volume; original simulation speed");
  Check(KeyboardInput.VirtualKey("Z")==90&&KeyboardInput.VirtualKey("UP")==38&&KeyboardInput.VirtualKey("4PAD")==100&&KeyboardInput.VirtualKey("unknown")==0,"Native keyboard profile conversion");passed.Add("Native keyboard profiles");
  Check(AudioOptions.Level(new Options{VolumePercent=0})==0&&AudioOptions.Level(new Options{VolumePercent=50})==50&&AudioOptions.Level(new Options{VolumePercent=100})==100,"Percent volume bounds");Check(AudioOptions.Level(new Options{Volume=-20})==10,"Legacy volume migration");passed.Add("Volume: 0/50/100 percent and migration of existing dB preference");
  var encoder=new InputPacketEncoder();encoder.Encode(1,100,[16,0,0,0],false);var released=encoder.Encode(2,110,[0,0,0,0],false).Split(' ',StringSplitOptions.RemoveEmptyEntries);Check(released[3]=="0"&&released[8]=="1","Released tap lost its edge");encoder.Reset();Check(encoder.Encode(1,120,[0,0,0,0],false).Split(' ')[8]=="0","New session replayed old presses");passed.Add("Short tap edge survives latest-state replacement; new session resets counters");
  var events=new SdlButtonEvents();const ulong now=1_000_000_000;
  events.Add(new(){Type=0x603,Timestamp=now,Which=7,Button=63},now);events.Add(new(){Type=0x603,Timestamp=now,Which=7,Button=64},now);events.Add(new(){Type=0x651,Timestamp=now,Which=8,Button=0},now);events.Add(new(){Type=0x652,Timestamp=now,Which=8,Button=0},now);
  Check(events.Raw(7)==1UL<<63&&events.Raw(8)==0&&events.Standard(8)==1&&events.Standard(7)==0,"Event namespaces / device IDs / raw bounds");
  events.Add(new(){Type=0x651,Timestamp=now-250_000_001,Which=8,Button=1},now);events.Add(new(){Type=0x651,Timestamp=now+1,Which=8,Button=2},now);Check(events.Standard(8)==1&&events.ExpiredEvents==2,"Old/future events must not replay");
  events.Add(new(){Type=0x650,Timestamp=now,Which=8,Button=4,AxisValue=14746},now);events.Add(new(){Type=0x650,Timestamp=now,Which=8,Button=5,AxisValue=14745},now);Check(events.Standard(8)==65,"Normalized trigger edge threshold");Check(PadLogic.Capture(new PadState{Buttons=[false]},new PadState{Buttons=[false],ButtonPresses=[true]})==new Binding("button",0),"Quick tap lost in binding capture");passed.Add("SDL events: separate players/raw/standard, button 63 bounds, expiry, no release replay, trigger threshold, short-tap binding capture");
  var dir=Path.Combine(AppContext.BaseDirectory,"verification");Directory.CreateDirectory(dir);
  var gamePath=Path.GetFullPath(Path.Combine(AppContext.BaseDirectory,"..","..","original","twcup98.zip"));if(File.Exists(gamePath)){var valid=RomService.Validate(gamePath,RomService.Game);Check(valid.Count==5,"Legacy names identified by content");var mapped=RomService.MapCartridge(valid);Check(mapped.SequenceEqual(File.ReadAllBytes(Path.Combine(Path.GetDirectoryName(gamePath)!,"../work/cart.bin"))),"Mapped cartridge differs from native reference");passed.Add("Actual supplied ROM: five chips and byte-identical native cartridge");}
  string bad=Path.Combine(dir,"bad.bin");File.WriteAllBytes(bad,[1,2,3]);bool rejected=false;try{RomService.Validate(bad,RomService.Game);}catch(InvalidDataException){rejected=true;}Check(rejected,"Invalid ROM rejected");passed.Add("Invalid ROM rejected");
  File.WriteAllText(Path.Combine(dir,"self-test.json"),JsonSerializer.Serialize(new{passed=true,checks=passed},new JsonSerializerOptions{WriteIndented=true}));
 }
}
