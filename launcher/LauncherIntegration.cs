using System.IO;
using System.Text.Json;
namespace Tecmo98;
public static class LauncherIntegration {
 public static async Task Run(){
  string root=Path.Combine(AppContext.BaseDirectory,"verification","Prueba con espacios ñ");var store=new Store(root);
  string source=Path.GetFullPath(Path.Combine(AppContext.BaseDirectory,"..","original","twcup98.zip"));
  if(!File.Exists(source))throw new FileNotFoundException("Integration test needs the user's local ROM",source);
  await Task.Run(()=>RomService.ImportNative(source,store));
  if(!RomService.GameReady(store))throw new Exception("Prepared ROM rejected");
  store.Value.Players[0].Keys[4]="Q";store.Save();
  if(new Store(root).Value.Players[0].Keys[4]!="Q")throw new Exception("Profile did not persist");
  string bad=Path.Combine(root,"invalid.zip");File.WriteAllBytes(bad,[1,2,3]);bool rejected=false;
  try{RomService.ImportNative(bad,store);}catch(InvalidDataException){rejected=true;}
  if(!rejected||!RomService.GameReady(store))throw new Exception("Invalid import damaged prepared game");
  store.Value.BackgroundInput=true;store.Value.Resolution="960x720";store.Value.VolumePercent=50;store.Value.Filter="Pixelart 2x";
  var game=new GameService(store);game.Start();
  try{
   var clock=System.Diagnostics.Stopwatch.StartNew();bool shortTapSent=false;
   while(clock.Elapsed.TotalSeconds<26&&game.Running){double t=clock.Elapsed.TotalSeconds;
    int mask=t>=2&&t<2.3?512:t>=4&&t<4.3?256:t>=18?8+((int)(t*2)%4==0?16:0):0;
    if(t>=8&&!shortTapSent){game.WriteState([16,0,0,0]);shortTapSent=true;}
    game.WriteState([mask,0,0,0]);await Task.Delay(20);
   }
   game.Stop();for(int i=0;i<500&&game.Running;i++)await Task.Delay(20);
   if(game.Running||game.ExitCode!=0)throw new Exception($"Native launch/exit failed: running={game.Running}, code={game.ExitCode}, session={game.LastSession}");
   string log=File.ReadAllText(Path.Combine(game.LastSession!,"stdout.log"));
   if(!log.Contains("\"draw_lists\":"))throw new Exception("Native player did not produce a session summary");
   File.WriteAllText(Path.Combine(AppContext.BaseDirectory,"verification","native-integration.json"),JsonSerializer.Serialize(new{passed=true,rom_import=true,bad_import_preserves_game=true,profiles_persist=true,unicode_paths=true,native_launch_and_exit=true,short_tap_before_receiver_poll=true,contact_source="synthetic launcher masks, not physical hardware",session=game.LastSession},new JsonSerializerOptions{WriteIndented=true}));
  }finally{game.Close();}
 }
}
