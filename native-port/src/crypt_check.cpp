#include "native_session.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

int main(int argc,char** argv){
    try{
        if(argc!=6){std::cerr<<"Usage: reference-crypt-accesses.csv EPROM raw-BIOS cartridge profile\n";return 2;}
        auto session=std::make_unique<tecmo98::NativeSession>(argv[2],argv[3],argv[4],argv[5],[]{return std::uint64_t(0);});
        std::ifstream in(argv[1]);std::string line;if(!std::getline(in,line)||line!="kind,address,data,mask")throw std::runtime_error("Wrong encrypted-stream reference");
        unsigned writes=0,reads=0,encrypted=0;std::uint32_t control=0;
        while(std::getline(in,line)){
            std::istringstream row(line);std::array<std::string,4> fields;for(auto& field:fields)if(!std::getline(row,field,','))throw std::runtime_error("Truncated crypt reference row");
            const auto address=std::uint32_t(std::stoul(fields[1],nullptr,16)),value=std::uint32_t(std::stoul(fields[2],nullptr,16)),mask=std::uint32_t(std::stoul(fields[3],nullptr,16));
            if(fields[0]=="W"){
                if(address==0x04fffff0)control=(control&~mask)|(value&mask);
                if(mask==0xffffffff)session->write(address,4,value);
                else for(unsigned half=0;half<2;++half){const unsigned shift=half==0?16:0;const auto lane=(mask>>shift)&0xffff;if(lane==0)continue;if(lane==0xffff)session->write(address+half*2,2,std::uint16_t(value>>shift));else for(unsigned byte=0;byte<2;++byte){const auto bit=shift+(byte==0?8:0);if(((mask>>bit)&255)!=0)session->write(address+half*2+byte,1,std::uint8_t(value>>bit));}}
                ++writes;
            }else if(fields[0]=="R"){
                const auto actual=std::uint32_t(session->read(address,4));
                if((actual&mask)!=(value&mask)){std::cerr<<std::hex<<"actual="<<actual<<" expected="<<value<<" mask="<<mask<<'\n';throw std::runtime_error("Crypt reference mismatch read="+std::to_string(reads)+" address="+std::to_string(address));}
                ++reads;if(address==0x04fffffc&&(control&0x10000)!=0)++encrypted;
            }else throw std::runtime_error("Unknown crypt reference operation");
        }
        if(writes<10||encrypted<1000)throw std::runtime_error("Crypt reference has insufficient real stream activity");
        std::cout<<"{\"passed\":true,\"writes\":"<<writes<<",\"reads\":"<<reads<<",\"encrypted_dword_reads\":"<<encrypted<<",\"reference\":\"separate stock MAME 0.289 process\",\"playable_game\":false}\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
