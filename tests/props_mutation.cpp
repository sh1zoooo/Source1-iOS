extern "C" int __lsan_is_turned_off(){return 1;}
extern "C" const char* __asan_default_options(){return "detect_leaks=0";}
#include "SourceProps.hpp"
#include <iostream>
#include <random>
int main(){
    std::vector<std::uint8_t> fixture(196);int one=1;std::memcpy(fixture.data(),&one,4);const char* name="models/probe.mdl";std::memcpy(fixture.data()+4,name,std::strlen(name));std::memcpy(fixture.data()+136,&one,4);
    std::vector<source1ios::PreviewProp> valid;if(!source1ios::parsePreviewProps(fixture,4,20,valid)||valid.size()!=1)return 1;
    std::mt19937 random(0x510524);unsigned accepted=0,rejected=0;
    for(unsigned i=0;i<10000;++i){auto bytes=fixture;if(i%5==0)bytes.resize(random()%(bytes.size()+1));else for(unsigned n=0;n<1+i%4;++n)bytes[random()%bytes.size()]^=1u<<(random()%8);
        std::vector<source1ios::PreviewProp> props(1);props[0].model="unchanged";
        if(source1ios::parsePreviewProps(bytes,4,20,props)){++accepted;for(const auto& p:props)for(float n:p.origin)if(!std::isfinite(n))return 2;}
        else{++rejected;if(props.size()!=1||props[0].model!="unchanged")return 3;}}
    std::cout<<accepted<<" accepted, "<<rejected<<" rejected; 10000 static prop mutations passed\n";
}
