using System.IO;
using System.Text.Json;
namespace Tecmo98;

// Small, local session log. No raw keyboard codes, game RAM, or per-packet disk log.
public sealed class InputHealth : IDisposable {
 readonly StreamWriter writer;
 long last,lastReport,samples,over50,over250,writeFailures,writeRetries;double gapMax;
 string previous="";
 string? lastWriteError;
 public InputHealth(string directory){writer=new(Path.Combine(directory,"input-health.jsonl"));writer.WriteLine(JsonSerializer.Serialize(new{version="0.4.6",target_interval_ms=8,ttl_ms=250}));writer.Flush();}
 public void WriteFailed(Exception error,string stage){writeFailures++;lastWriteError=$"{stage} 0x{error.HResult:X8}: {error.Message}";}
 public void WriteRetry()=>writeRetries++;
 public void Sample(long now,bool focused,bool blocked,int connected,string? error,long recovered,long expired){
  if(last!=0){var gap=now-last;gapMax=Math.Max(gapMax,gap);if(gap>50)over50++;if(gap>250)over250++;}last=now;samples++;
  string state=$"{focused}/{blocked}/{connected}/{error}";
  if(now-lastReport<1000&&previous==state)return;
  writer.WriteLine(JsonSerializer.Serialize(new{milliseconds=now,samples,gap_max_ms=gapMax,gaps_over_50_ms=over50,gaps_over_250_ms=over250,focused,blocked,assigned_connected=connected,error,queued_buttons_recovered=recovered,expired_events=expired,write_failures=writeFailures,write_retries=writeRetries,last_write_error=lastWriteError}));writer.Flush();lastReport=now;previous=state;
 }
 public void Dispose(){writer.WriteLine(JsonSerializer.Serialize(new{final=true,samples,gap_max_ms=gapMax,gaps_over_50_ms=over50,gaps_over_250_ms=over250,write_failures=writeFailures,write_retries=writeRetries,last_write_error=lastWriteError}));writer.Dispose();}
}
