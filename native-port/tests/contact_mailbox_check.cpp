#include "native_contact_mailbox.hpp"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
int main(){try{
 tecmo98::NativeContactMailbox contacts;std::array<std::uint8_t,7> pressed{1,2,3,4,5,6,7},neutral{};
 require(contacts.read(1000,true)==neutral,"Uninitialized contacts");contacts.publish(pressed,1000);
 require(contacts.read(1250,true)==pressed,"TTL boundary changed");require(contacts.read(1251,true)==neutral,"Expired contacts stuck");require(contacts.read(999,true)==neutral,"Future contacts accepted");require(contacts.read(1001,false)==neutral,"Focus does not release contacts");
 require(contacts.read(1001,true)==pressed,"Reading neutral mutated source");
 tecmo98::NativePacketMailbox packets;require(packets.read().empty(),"Uninitialized packet");packets.publish("first");auto copy=packets.read();packets.publish("second");require(copy=="first"&&packets.read()=="second","Packet snapshot mutated");
 std::atomic<bool> done{false};contacts.publish(neutral,1000);packets.publish(std::string(900,'a'));
 std::jthread writer([&]{for(unsigned i=0;i<20000;++i){std::array<std::uint8_t,7> v;v.fill(std::uint8_t(i%250+1));contacts.publish(v,1000);packets.publish(std::string(900,char('a'+i%20)));}done=true;});
 unsigned observations=0;while(!done){auto v=contacts.read(1000,true);require(std::all_of(v.begin(),v.end(),[&](auto x){return x==v[0];}),"Torn contact snapshot");auto p=packets.read();require(p.size()==900&&std::all_of(p.begin(),p.end(),[&](auto c){return c==p[0];}),"Torn packet snapshot");++observations;}
 writer.join();require(observations>0,"No concurrent observations");std::cout<<"{\"passed\":true,\"ttl_focus_and_owned_copies\":true,\"concurrent_writes\":20000,\"coherent_observations\":"<<observations<<"}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
