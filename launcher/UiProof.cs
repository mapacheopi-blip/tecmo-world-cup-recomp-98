using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Input;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
namespace Tecmo98;
public partial class MainWindow {
 public async Task WriteUiProof(bool importRom=true){
  timer.Stop();var dir=Path.Combine(AppContext.BaseDirectory,"verification","ui-v047");Directory.CreateDirectory(dir);int screens=0;
  void Check(bool ok,string message){if(!ok)throw new Exception(message);}
  IEnumerable<DependencyObject> Descendants(DependencyObject root){for(int i=0;i<VisualTreeHelper.GetChildrenCount(root);i++){var child=VisualTreeHelper.GetChild(root,i);yield return child;foreach(var nested in Descendants(child))yield return nested;}}
  async Task Snap(string name){
   await Dispatcher.InvokeAsync(UpdateLayout,DispatcherPriority.ApplicationIdle);
   await Task.Delay(80); // Let WPF present updated glyphs before capturing a language change.
   var content=(FrameworkElement)Content;int w=(int)content.ActualWidth,h=(int)content.ActualHeight;
   Check(LanguageSelect.IsVisible&&LanguageSelect.TranslatePoint(new Point(),content).Y>=0,"Language selector must stay at the top");
   var bitmap=new RenderTargetBitmap(w,h,96,96,PixelFormats.Pbgra32);var visual=new DrawingVisual();using(var dc=visual.RenderOpen()){dc.DrawRectangle(Brush("#09090D"),null,new Rect(0,0,w,h));dc.DrawRectangle(new VisualBrush(content){Stretch=Stretch.Fill},null,new Rect(0,0,w,h));}bitmap.Render(visual);var encoder=new PngBitmapEncoder();encoder.Frames.Add(BitmapFrame.Create(bitmap));using var file=File.Create(Path.Combine(dir,name+".png"));encoder.Save(file);screens++;
  }
  Check(L.Language=="en","Fresh installation should default to English");
  Navigate("Inicio");Check((string?)play?.Content=="Select ROM","Fresh English home must request ROM");await Snap("en-home-select-rom");
  LanguageSelect.SelectedValue="es";Check((string?)play?.Content=="Seleccionar ROM","Home must switch immediately to Spanish");await Snap("es-home-select-rom");
  if(importRom){string rom=Path.GetFullPath(Path.Combine(AppContext.BaseDirectory,"..","original","twcup98.zip"));await Task.Run(()=>RomService.ImportNative(rom,store));Navigate("Inicio");Check((string?)play?.Content=="Start","Valid ROM must become Start");}
  // Layout/input fixture only; this is not a physical controller test.
  pads=PadLogic.Parse("[{\"index\":99,\"vendorId\":\"054c\",\"productId\":\"0ce6\",\"name\":\"DualSense · layout fixture\",\"buttonCount\":15,\"axes\":[0,0,0,0],\"hats\":[0],\"standardButtons\":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],\"standardAxes\":[0,0,0,0]}]");
  store.Value.Players[0].Device=pads[0].Key;store.Value.Pads[pads[0].Key]=PadLogic.Default(pads[0]);
  foreach(string language in new[]{"en","es"}){
   LanguageSelect.SelectedValue=language;Width=1280;Height=820;Navigate("Inicio");await Snap(language+"-home");
   Navigate("General");await Snap(language+"-general");
   Navigate("Controles");deviceBox!.Focus();pads[0].Buttons[13]=true;menuMask=0;NavigateWithPad();Check(Keyboard.FocusedElement!=deviceBox,"Controller focus trapped in device selector");pads[0].Buttons[13]=false;menuMask=0;await Snap(language+"-controls-pad");
   keyboardMode=true;Navigate("Controles");await Snap(language+"-controls-keyboard");keyboardMode=false;
   Navigate("Gráficos");
   Descendants(Page).OfType<RadioButton>().Single(b=>(string)b.Content==L.T("Suave")).IsChecked=true;
   Check(store.Value.Filter=="Suave","Translated filter label leaked into persisted/native settings");await Snap(language+"-graphics");
   Navigate("Sonido");await Snap(language+"-sound");
   Width=1024;Height=720;Navigate("Controles");await Snap(language+"-controls-1024");
   foreach(Button b in Navigation.Children)Check((string)b.Content==L.T((string)b.Tag),"Navigation lost its stable page key");
  }
  var before=JsonSerializer.Serialize(store.Value.Players);var profileBefore=JsonSerializer.Serialize(store.Value.Pads);
  BeginCapture(4,false);LanguageSelect.SelectedValue="en";Check(capture==-1,"Changing language must cancel pending binding capture");
  Check(JsonSerializer.Serialize(store.Value.Players)==before&&JsonSerializer.Serialize(store.Value.Pads)==profileBefore,"Language switch changed a controller profile");
  string settings=GameService.BuildSettings(store.Value);LanguageSelect.Focus();pads[0].Buttons[15]=true;menuMask=0;NavigateWithPad();Check(L.Language=="es","Controller right must switch language in focused selector");pads[0].Buttons[15]=false;menuMask=0;
  Check(GameService.BuildSettings(store.Value)==settings,"Language changed native launch protocol");
  Check(new Store(store.Root).Value.Language=="es","Language preference did not persist");
  Check(L.T("No se puede iniciar: ROM del juego")=="No se puede iniciar: ROM del juego","Spanish error details changed");
  LanguageSelect.SelectedValue="en";Check(L.T("No se puede iniciar: ROM del juego")=="Cannot start: Game ROM","English error details were not translated");
  SetStatus("An external error may contain {braces}.");Check(Status.Text.Contains("{braces}"),"Raw driver errors must not become format strings");
  File.WriteAllText(Path.Combine(dir,"result.json"),JsonSerializer.Serialize(new{passed=true,languages=new[]{"en","es"},screens,imported_rom=importRom,language_persisted=true,language_selector_controller_navigation=true,capture_cancelled_without_rebinding=true,native_settings_unchanged=true,sizes=new[]{"1280x820","1024x720"}},new JsonSerializerOptions{WriteIndented=true}));
 }
}
