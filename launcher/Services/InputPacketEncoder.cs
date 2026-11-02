namespace Tecmo98;
// Monotonic button edge counters survive replacement of a press by its release.
public sealed class InputPacketEncoder {
 readonly uint[] edges=new uint[24];readonly int[] previous=new int[4];
 public void Reset(){Array.Clear(edges);Array.Clear(previous);}
 public string Encode(long sequence,long milliseconds,int[] masks,bool quit){
  for(int p=0;p<4;p++){for(int a=4;a<10;a++)if((masks[p]&(1<<a))!=0&&(previous[p]&(1<<a))==0)edges[p*6+a-4]=unchecked(edges[p*6+a-4]+1);previous[p]=masks[p];}
  return $"P2 {sequence} {milliseconds} {masks[0]} {masks[1]} {masks[2]} {masks[3]} {(quit?1:0)} {string.Join(' ',edges)}\n";
 }
}
