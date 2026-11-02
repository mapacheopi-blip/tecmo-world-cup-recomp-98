using System.IO;
using System.Diagnostics;
using System.Runtime.InteropServices;
namespace Tecmo98;
public sealed class GameService {
 readonly Store store;Process? process;string? session;long seq;bool stopping;readonly InputPacketEncoder encoder=new();InputHealth? health;
 public bool Running=>process!=null&&!process.HasExited;
 public string? LastSession=>session;
 [DllImport("user32.dll")]static extern nint GetForegroundWindow();
 [DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(nint window,out uint pid);
 public bool GameFocused {get{if(!Running)return false;GetWindowThreadProcessId(GetForegroundWindow(),out var foregroundPid);return foregroundPid==(uint)process!.Id;}}
 public GameService(Store store){this.store=store;}
 public static List<string> BuildArgs(Store s)=>[Path.Combine(s.NativeData,"boot-epr.bin"),Path.Combine(s.Documents,"bios-jp1.bin"),Path.Combine(s.NativeData,"cart.bin"),Path.Combine(s.Documents,"resident-profile.bin")];
 public static string BuildSettings(Options o){
  var sizes=new Dictionary<string,(int,int)>{{"960x720",(960,720)},{"1280x720",(1280,720)},{"1280x960",(1280,960)},{"1600x900",(1600,900)},{"1920x1080",(1920,1080)},{"2560x1440",(2560,1440)},{"3840x2160",(3840,2160)}};
  var (w,h)=sizes.GetValueOrDefault(o.Resolution,(1280,720));
  return $"TECMO98_NATIVE_V2 {w} {h} {(o.Fullscreen?1:0)} {(o.Aspect=="Estirar"?1:0)} {(o.Filter=="Suave"?1:o.Filter=="Pixelart 2x"?2:0)} {AudioOptions.Level(o)} {(o.Muted?1:0)} {(o.BackgroundInput?1:0)}\n";
 }
 public void Start(){
  if(Running)throw new InvalidOperationException("Ya hay una partida abierta.");
  var missing=RomService.Missing(store);if(missing.Length>0)throw new InvalidOperationException("Falta preparar: "+string.Join(", ",missing));
  store.Save();stopping=false;encoder.Reset();session=Path.Combine(store.Runtime,"sessions",DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N")[..6]);Directory.CreateDirectory(session);
  health?.Dispose();health=new(session);File.WriteAllText(Path.Combine(session,"launch.cfg"),BuildSettings(store.Value));WriteState(new int[4]);
  var info=new ProcessStartInfo(store.Player){UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=session,RedirectStandardError=true,RedirectStandardOutput=true};
  foreach(var arg in BuildArgs(store))info.ArgumentList.Add(arg);
  info.Environment["TECMO98_LAUNCHER_CONFIG"]=Path.Combine(session,"launch.cfg");info.Environment["TECMO98_INPUT_FILE"]=Path.Combine(session,"input.txt");
  File.WriteAllText(Path.Combine(session,"arguments.json"),System.Text.Json.JsonSerializer.Serialize(info.ArgumentList.ToArray()));
  process?.Dispose();process=new Process{StartInfo=info};
  var logRoot=session;process.OutputDataReceived+=(_,e)=>Log(logRoot,"stdout.log",e.Data);process.ErrorDataReceived+=(_,e)=>Log(logRoot,"stderr.log",e.Data);
  process.Start();process.BeginOutputReadLine();process.BeginErrorReadLine();
 }
 static void Log(string root,string file,string? text){if(text==null)return;try{lock(typeof(GameService))File.AppendAllText(Path.Combine(root,file),text+Environment.NewLine);}catch(IOException){}}
 public void WriteState(int[] masks,bool quit=false){
  if(session==null)return;if(masks.Length!=4||masks.Any(m=>m<0||m>1023))throw new ArgumentException("Invalid input masks");
  var path=Path.Combine(session,"input.txt");string stage="write";try{
   File.WriteAllText(path+".tmp",encoder.Encode(++seq,Environment.TickCount64,masks,quit||stopping));stage="replace";
   // Replacement can be transiently denied on Windows. Retry this SAME packet,
   // without duplicating its sequence or edges; persistent errors remain logged.
   for(int attempt=0;;attempt++)try{File.Move(path+".tmp",path,true);break;}
    catch(Exception ex)when(attempt<2&&(ex is IOException||ex is UnauthorizedAccessException)&&((ex.HResult&0xffff) is 5 or 32 or 33)){health?.WriteRetry();System.Threading.Thread.Sleep(1);}
  }catch(IOException ex){health?.WriteFailed(ex,stage);}catch(UnauthorizedAccessException ex){health?.WriteFailed(ex,stage);}
 }
 public void NoteInput(bool focused,bool blocked,int connected,string? error)=>health?.Sample(Environment.TickCount64,focused,blocked,connected,error,ArenaSDLControls.RecoveredButtons,ArenaSDLControls.ExpiredEvents);
 public void FinishInputLog(){health?.Dispose();health=null;}
 public void Stop(){
  stopping=true;WriteState(new int[4],true);
  // Closing our own game's window is another graceful request, independent of
  // transient file replacement failures. Never terminate the child process.
  try{if(Running)process!.CloseMainWindow();}catch(InvalidOperationException){}
 }
 public void Close(){Stop();FinishInputLog();process?.Dispose();process=null;}
 public int? ExitCode=>process?.HasExited==true?process.ExitCode:null;
}
