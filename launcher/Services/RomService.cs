using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;
namespace Tecmo98;
public static class RomService {
 public static readonly (string Name,string Sha1,int Size)[] Game=[("epr20819.24","f66cc955181720661a0334fe67fa5750ddf9758b",1048576),("mpr20821.12","5fcaf4257f3639cb3aa407d2936f616499a09d97",4194304),("mpr20822.13","d5689ac8aad63509febe9aa4077351be09b2d8d4",4194304),("mpr20823.14","c387d03ba27580c62ac0bf780915fdf41552df6f",4194304),("mpr20824.15","310961a5f114fea8938a3f514dffd5231e910a5a",4194304)];
 public static readonly (string Name,string Sha1,int Size)[] Bios=[("epr-20091.ic8","eff0f54c70bce05ff3a289bf30b1027e1c8cd117",524288)];
 public static readonly (string Name,string Sha1,int Size)[] Aux=[("epr-18022.ic2","edf5ade72d9fa2f4d5f83f9f89e6cecfadd77f56",65536)];
 public static string Hash(byte[] data)=>Convert.ToHexString(SHA1.HashData(data)).ToLowerInvariant();
 public static Dictionary<string,byte[]> Validate(string path,(string Name,string Sha1,int Size)[] required){
  var found=new Dictionary<string,byte[]>();if(!File.Exists(path))throw new InvalidDataException("Selecciona un archivo que exista.");
  if(Path.GetExtension(path).Equals(".zip",StringComparison.OrdinalIgnoreCase)){
   using var zip=ZipFile.OpenRead(path);if(zip.Entries.Count>512)throw new InvalidDataException("El ZIP contiene demasiados archivos para este juego.");
   foreach(var e in zip.Entries){if(e.Length>8388608||!required.Any(r=>r.Size==e.Length))continue;using var s=e.Open();using var m=new MemoryStream();s.CopyTo(m);var data=m.ToArray();var hash=Hash(data);foreach(var r in required)if(r.Size==data.Length&&r.Sha1==hash)found[r.Name]=data;}
  }else{var size=new FileInfo(path).Length;if(size>8388608)throw new InvalidDataException("El archivo no tiene el tamaño esperado.");var data=File.ReadAllBytes(path);var hash=Hash(data);foreach(var r in required)if(r.Size==data.Length&&r.Sha1==hash)found[r.Name]=data;}
  var missing=required.Where(r=>!found.ContainsKey(r.Name)).Select(r=>r.Name).ToArray();if(missing.Length>0)throw new InvalidDataException("No coincide con la revisión compatible. Faltan o difieren: "+string.Join(", ",missing));return found;
 }
 public static void Import(string source,string destination,(string Name,string Sha1,int Size)[] required){var data=Validate(source,required);Directory.CreateDirectory(Path.GetDirectoryName(destination)!);string tmp=destination+".tmp";using(var z=ZipFile.Open(tmp,ZipArchiveMode.Create))foreach(var e in data){using var s=z.CreateEntry(e.Key,CompressionLevel.Optimal).Open();s.Write(e.Value);}File.Move(tmp,destination,true);}
 public static bool Good(string path,(string Name,string Sha1,int Size)[] required){try{Validate(path,required);return true;}catch{return false;}}
 public static bool FileGood(string path,int size,string sha1){try{if(new FileInfo(path).Length!=size)return false;using var stream=File.OpenRead(path);return Convert.ToHexString(SHA1.HashData(stream)).Equals(sha1,StringComparison.OrdinalIgnoreCase);}catch(IOException){return false;}catch(UnauthorizedAccessException){return false;}}
 public static bool GameReady(Store s)=>FileGood(Path.Combine(s.NativeData,"boot-epr.bin"),1048576,Game[0].Sha1)&&FileGood(Path.Combine(s.NativeData,"cart.bin"),0x3000000,"2e4f293a0b6cc5b32694671cd4d9ea20381add88");
 public static byte[] MapCartridge(Dictionary<string,byte[]> chips){
  var image=new byte[0x3000000];var epr=chips[Game[0].Name];
  for(int i=0;i<epr.Length;i++)image[1+i*2]=epr[i];
  epr.CopyTo(image,0x200000);epr.CopyTo(image,0x300000);
  for(int chip=1;chip<Game.Length;chip++){var data=chips[Game[chip].Name];int offset=0x400000+(chip-1)*0x400000;for(int i=0;i<data.Length;i+=2){image[offset+i]=data[i+1];image[offset+i+1]=data[i];}}
  if(Hash(image)!="2e4f293a0b6cc5b32694671cd4d9ea20381add88")throw new InvalidDataException("El cartucho preparado no coincide con la revisión compatible.");return image;
 }
 public static void ImportNative(string source,Store store){
  var chips=Validate(source,Game);var image=MapCartridge(chips);
  // Validate everything before replacing any installed asset. Originals are read-only.
  var eprom=Path.Combine(store.NativeData,"boot-epr.bin");var cart=Path.Combine(store.NativeData,"cart.bin");
  File.WriteAllBytes(eprom+".tmp",chips[Game[0].Name]);File.WriteAllBytes(cart+".tmp",image);
  File.Move(eprom+".tmp",eprom,true);File.Move(cart+".tmp",cart,true);
 }
 public static string[] Missing(Store s){var a=new List<string>();if(!GameReady(s))a.Add("ROM del juego");if(!FileGood(Path.Combine(s.Documents,"bios-jp1.bin"),524288,Bios[0].Sha1))a.Add("BIOS en Documentos");if(!FileGood(Path.Combine(s.Documents,"resident-profile.bin"),16384,"b985e6bd21be05a0b5187150498e0ce7aa93b13e")&&!FileGood(Path.Combine(s.Documents,"resident-profile.bin"),1048576,"69538b162c56a1075007ab392630070fd067683a"))a.Add("Perfil de arranque en Documentos");if(!File.Exists(s.Player))a.Add("Ejecutable nativo");return a.ToArray();}
}
