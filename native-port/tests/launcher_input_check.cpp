#include "native_launcher.hpp"
#include <iostream>
static void check(bool x,const char* m){if(!x)throw std::runtime_error(m);}
int main(){try{
 tecmo98::LauncherInput input;
 std::istringstream a("1 785 96 272 256 0");check(input.accept(a,100),"Valid packet rejected");
 auto c=input.contacts(100,true);check(c[0]==0x21&&c[2]==0x11&&c[1]==6&&c[4]==9&&c[5]==8,"Four-player JAMMA mapping changed");
 check(input.contacts(351,true)==std::array<std::uint8_t,7>{},"Stale inputs stuck");
 check(input.contacts(110,false)==std::array<std::uint8_t,7>{},"Focus loss held input");
 std::istringstream repeat("1 785 96 272 256 0");check(!input.accept(repeat,400),"Replay renewed heartbeat");
 for(auto packet:{"2 0 0 0 1024 0","2 1 0 0 0 0 trailing","2 1 0","2 0 0 0 0 2"}){std::istringstream s(packet);check(!input.accept(s,400),"Malformed packet accepted");}
 std::istringstream opposite("2 15 0 0 0 0");check(input.accept(opposite,400)&&input.contacts(400,true)[0]==0,"Opposite directions not neutral");
 std::istringstream quit("3 0 0 0 0 1");check(input.accept(quit,410)&&input.quit(),"Graceful quit missing");
 const auto packet=[](unsigned seq,unsigned stamp,unsigned mask,unsigned presses){std::ostringstream s;s<<"P2 "<<seq<<' '<<stamp<<' '<<mask<<" 0 0 0 0 "<<presses;for(unsigned i=1;i<24;++i)s<<" 0";return s.str();};
 tecmo98::LauncherInput events;
 std::istringstream releaseOnly(packet(1,100,0,1));check(events.accept(releaseOnly,105)&&events.contacts(105,true)[0]==1,"Short tap overwritten before receiver read was lost");
 check(events.contacts(140,true)[0]==0&&events.recoveredTaps()==1,"Recovered tap stuck or not accounted");
 std::istringstream repeatCount(packet(2,150,0,1));check(events.accept(repeatCount,150)&&events.contacts(150,true)[0]==0,"Old edge replayed");
 std::istringstream secondTap(packet(3,160,16,2));check(events.accept(secondTap,160)&&events.contacts(160,true)[0]==1,"New button edge lost");
 check(events.contacts(161,false)[0]==0,"Focus did not release tap");
 std::istringstream released(packet(4,162,0,2));check(events.accept(released,162)&&events.contacts(162,true)[0]==0,"Focus return replayed tap");
 std::istringstream stale(packet(5,170,16,3));check(!events.accept(stale,500),"Old sender sample accepted");
 std::istringstream malformed("P2 6 500 0 0 0 0 0 1");check(!events.accept(malformed,500),"Truncated edge packet accepted");
 std::istringstream good("TECMO98_NATIVE_V1 1280 720 1 0 1 -12 0 1");auto settings=tecmo98::LaunchSettings::read(good);check(settings.fullscreen==1&&settings.volumeDb==-12,"Settings changed");
 for(int level:{0,50,100}){std::istringstream s("TECMO98_NATIVE_V2 960 720 0 0 2 "+std::to_string(level)+" 0 0");auto v=tecmo98::LaunchSettings::read(s);check(v.gain()==double(level)/100&&v.smooth==2,"Percent gain or pixel filter changed");}
 for(auto text:{"TECMO98_NATIVE_V2 960 720 0 0 3 100 0 0","TECMO98_NATIVE_V2 960 720 0 0 0 101 0 0","TECMO98_NATIVE_V2 960 720 0 0 0 -1 0 0"}){bool rejected=false;try{std::istringstream s(text);tecmo98::LaunchSettings::read(s);}catch(...){rejected=true;}check(rejected,"Invalid V2 settings accepted");}
 for(auto text:{"bad","TECMO98_NATIVE_V1 1280 720 2 0 1 0 0 1","TECMO98_NATIVE_V1 1280 720 0 0 1 1 0 1"}){bool rejected=false;try{std::istringstream s(text);tecmo98::LaunchSettings::read(s);}catch(...){rejected=true;}check(rejected,"Invalid settings accepted");}
 std::cout<<"{\"passed\":true,\"four_players\":true,\"heartbeat_and_focus\":true,\"malformed_packets\":true,\"socd\":true,\"settings_bounds\":true,\"graceful_quit\":true}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
