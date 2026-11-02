using System.Windows;
namespace Tecmo98;
public partial class App : Application {
 protected override async void OnStartup(StartupEventArgs e) {
  base.OnStartup(e);
  if(e.Args.Contains("--sdl-input-check")||e.Args.Contains("--sdl-input-match")){ShutdownMode=ShutdownMode.OnExplicitShutdown;try{if(e.Args.Contains("--sdl-input-match"))await SdlInputRegression.Match(e.Args.Contains("--quick"),e.Args.Contains("--deny-close-packet"),e.Args.Contains("--sharp"),e.Args.Contains("--long"));else SdlInputRegression.Run();Shutdown(0);}catch(Exception ex){System.IO.File.WriteAllText(System.IO.Path.Combine(AppContext.BaseDirectory,"sdl-input-error.txt"),ex.ToString());Shutdown(1);}return;}
  if(e.Args.Contains("--ui-proof")||e.Args.Contains("--localization-check")){ShutdownMode=ShutdownMode.OnExplicitShutdown;MainWindow? proof=null;try{proof=new MainWindow(new Store(System.IO.Path.Combine(AppContext.BaseDirectory,"verification","ui-state-"+Guid.NewGuid().ToString("N"))));proof.Show();await System.Windows.Threading.Dispatcher.Yield(System.Windows.Threading.DispatcherPriority.ApplicationIdle);await proof.WriteUiProof(!e.Args.Contains("--localization-check"));proof.Close();Shutdown(0);}catch(Exception ex){System.IO.File.WriteAllText(System.IO.Path.Combine(AppContext.BaseDirectory,"ui-proof-error.txt"),ex.ToString());proof?.Close();Shutdown(1);}return;}
  if(e.Args.Contains("--integration-test")){ShutdownMode=ShutdownMode.OnExplicitShutdown;try{await LauncherIntegration.Run();Shutdown(0);}catch(Exception ex){System.IO.File.WriteAllText(System.IO.Path.Combine(AppContext.BaseDirectory,"integration-error.txt"),ex.ToString());Shutdown(1);}return;}
  if(e.Args.Contains("--probe-input")){ShutdownMode=ShutdownMode.OnExplicitShutdown;try{using var input=new InputService();var samples=new System.Collections.Generic.List<object>();string backend=e.Args.Contains("--directinput")?"DirectInput":"SDL";for(int i=0;i<150;i++){var pads=input.Poll(backend);if(i%10==0)samples.Add(new{elapsed=i*20,backend,error=input.Error,devices=pads.Select(p=>new{p.Name,p.Driver,p.Standard,p.Raw})});System.Threading.Thread.Sleep(20);}System.IO.File.WriteAllText(System.IO.Path.Combine(AppContext.BaseDirectory,"input-probe-"+backend+".json"),System.Text.Json.JsonSerializer.Serialize(samples,new System.Text.Json.JsonSerializerOptions{WriteIndented=true}));Shutdown(0);}catch(Exception ex){System.IO.File.WriteAllText(System.IO.Path.Combine(AppContext.BaseDirectory,"input-probe-error.txt"),ex.ToString());Shutdown(1);}return;}
  if(e.Args.Contains("--self-test")){try{SelfTest.Run();Shutdown(0);}catch(Exception ex){System.IO.File.WriteAllText(System.IO.Path.Combine(AppContext.BaseDirectory,"self-test-error.txt"),ex.ToString());Shutdown(1);}return;}
  DispatcherUnhandledException += (_,a)=>{MessageBox.Show(a.Exception.Message,"TECMO WORLD CUP RECOMP 98",MessageBoxButton.OK,MessageBoxImage.Error);a.Handled=true;};
  var w=new MainWindow();MainWindow=w;w.Show();
 }
}

