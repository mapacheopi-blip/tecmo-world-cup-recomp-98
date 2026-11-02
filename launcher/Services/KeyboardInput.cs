using System.Runtime.InteropServices;
namespace Tecmo98;
public static class KeyboardInput {
 [DllImport("user32.dll")]static extern short GetAsyncKeyState(int key);
 public static int VirtualKey(string code){
  if(code.Length==1&&char.IsAsciiLetterOrDigit(code[0]))return char.ToUpperInvariant(code[0]);
  if(code.Length==4&&code.EndsWith("PAD")&&char.IsAsciiDigit(code[0]))return 0x60+code[0]-'0';
  return code switch{"UP"=>0x26,"DOWN"=>0x28,"LEFT"=>0x25,"RIGHT"=>0x27,"SPACE"=>0x20,"LCONTROL"=>0xa2,"RCONTROL"=>0xa3,"LALT"=>0xa4,"RALT"=>0xa5,"LSHIFT"=>0xa0,"RSHIFT"=>0xa1,"ENTER" or "ENTERPAD"=>0x0d,"TAB"=>9,"BACKSPACE"=>8,"DELPAD"=>0x6e,"PLUSPAD"=>0x6b,"MINUSPAD"=>0x6d,_=>0};
 }
 public static int Read(PlayerProfile p){int mask=0;for(int a=0;a<10;a++){int key=VirtualKey(p.Keys[a]);if(key!=0&&(GetAsyncKeyState(key)&0x8000)!=0)mask|=1<<a;}return mask;}
}
