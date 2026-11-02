using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Input;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
namespace Tecmo98;
public partial class MainWindow {
 public async Task WriteUiProof(){
  timer.Stop();var dir=Path.Combine(AppContext.BaseDirectory,"verification","ui-v04");Directory.CreateDirectory(dir);
  async Task Snap(string name){await Dispatcher.InvokeAsync(UpdateLayout,DispatcherPriority.ApplicationIdle);var content=(FrameworkElement)Content;int w=(int)content.ActualWidth,h=(int)content.ActualHeight;var bitmap=new RenderTargetBitmap(w,h,96,96,PixelFormats.Pbgra32);var background=new DrawingVisual();using(var dc=background.RenderOpen())dc.DrawRectangle(Brush("#09090D"),null,new Rect(0,0,w,h));bitmap.Render(background);var visual=new DrawingVisual();using(var dc=visual.RenderOpen())dc.DrawRectangle(new VisualBrush(content){Stretch=Stretch.Fill},null,new Rect(0,0,w,h));bitmap.Render(visual);var encoder=new PngBitmapEncoder();encoder.Frames.Add(BitmapFrame.Create(bitmap));using var file=File.Create(Path.Combine(dir,name+".png"));encoder.Save(file);}
  Navigate("Inicio");if((string?)play?.Content!="Seleccionar ROM")throw new Exception("Fresh home did not request ROM");await Snap("01-home-select-rom");
  string rom=Path.GetFullPath(Path.Combine(AppContext.BaseDirectory,"..","original","twcup98.zip"));await Task.Run(()=>RomService.ImportNative(rom,store));Navigate("Inicio");if((string?)play?.Content!="Start")throw new Exception("Valid ROM did not become Start");await Snap("02-home-start");
  // Explicit layout fixture; not evidence of a physical controller signal.
  pads=PadLogic.Parse("[{\"index\":99,\"vendorId\":\"054c\",\"productId\":\"0ce6\",\"name\":\"DualSense · vista de prueba\",\"buttonCount\":15,\"axes\":[0,0,0,0],\"hats\":[0],\"standardButtons\":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],\"standardAxes\":[0,0,0,0]}]");store.Value.Players[0].Device=pads[0].Key;store.Value.Pads[pads[0].Key]=PadLogic.Default(pads[0]);
  Navigate("Controles");deviceBox!.Focus();pads[0].Buttons[13]=true;menuMask=0;NavigateWithPad();if(Keyboard.FocusedElement==deviceBox)throw new Exception("Gamepad focus trapped in device selector");pads[0].Buttons[13]=false;menuMask=0;await Snap("03-controls-pad");keyboardMode=true;Navigate("Controles");await Snap("04-controls-keyboard");keyboardMode=false;
  Navigate("Gráficos");await Snap("05-graphics");Navigate("Sonido");await Snap("06-sound");
  store.Value.VolumePercent=0;Navigate("Sonido");await Snap("07-sound-zero");store.Value.VolumePercent=50;store.Value.Muted=true;Navigate("Sonido");await Snap("08-sound-mute");
  Width=1024;Height=720;Navigate("Controles");await Snap("09-controls-1024");
  File.WriteAllText(Path.Combine(dir,"result.json"),"{\"passed\":true,\"select_rom_becomes_start\":true,\"controller_layout_fixture\":true,\"sizes\":[\"1280x820\",\"1024x720\"],\"screens\":9}");
 }
}
