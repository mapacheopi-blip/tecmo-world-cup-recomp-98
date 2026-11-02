namespace Tecmo98;
public static class AudioOptions {
 public static int Level(Options o)=>Math.Clamp(o.VolumePercent??(int)Math.Round(Math.Pow(10,Math.Clamp(o.Volume,-32,0)/20d)*100),0,100);
}
