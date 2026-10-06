extern "C" int __lsan_is_turned_off(){return 1;}
extern "C" const char* __asan_default_options(){return "detect_leaks=0";}
#include "SourcePhy.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
int main(){
    constexpr int checksum=0x510510;constexpr size_t surface=48,base=48,node=48,ledge=80,points=160,bytes=224;
    std::vector<std::uint8_t> fixture(20+28+bytes+1);
    auto put=[&](size_t at,auto value){std::memcpy(fixture.data()+at,&value,sizeof(value));};
    put(0,16);put(8,1);put(12,checksum);put(16,int(28+bytes));put(20,source1ios::phy_detail::id('V','P','H','Y'));put(24,std::uint16_t(0x100));put(26,std::uint16_t(0));put(28,int(bytes));
    put(base+12,1.f);put(base+16,1.f);put(base+20,1.f);put(base+24,2.f);put(base+28,std::uint32_t(bytes<<8));put(base+32,int(node));put(base+44,source1ios::phy_detail::id('I','V','P','S'));
    put(base+node+4,int(ledge-node));put(base+node+20,2.f);put(base+ledge,int(points-ledge));put(base+ledge+8,std::uint32_t((9<<8)|4));put(base+ledge+12,std::int16_t(4));
    const unsigned triangles[4][3]={{0,2,1},{0,1,3},{1,2,3},{2,0,3}};
    for(unsigned t=0;t<4;++t){put(base+ledge+16+t*16,std::uint32_t(t|((t+1)%4<<12)));
        for(unsigned e=0;e<3;++e){const size_t at=ledge+20+t*16+e*4;int opposite=0;
            for(unsigned other=0;other<4;++other)for(unsigned oe=0;oe<3;++oe)if(triangles[other][oe]==triangles[t][(e+1)%3]&&triangles[other][(oe+1)%3]==triangles[t][e])opposite=(int(ledge+20+other*16+oe*4)-int(at))/4;
            put(base+at,std::uint32_t(triangles[t][e]|((std::uint32_t(opposite)&0x7fff)<<16)));}}
    put(base+points+16,1.f);put(base+points+32+4,1.f);put(base+points+48+8,1.f);
    source1ios::PhyGeometry result;std::string error;if(!source1ios::parsePhy(fixture,checksum,result,error)||result.convexes.size()!=1||result.convexes[0].size()!=4){std::cerr<<error<<'\n';return 1;}
    std::mt19937 random(0x510527);unsigned accepted=0,rejected=0;
    for(unsigned i=0;i<20000;++i){auto data=fixture;if(i%5==0)data.resize(random()%(data.size()+1));else for(unsigned n=0;n<1+i%5;++n)data[random()%data.size()]^=1u<<(random()%8);
        source1ios::PhyGeometry out;out.convexes={{{1,2,3}}};
        if(source1ios::parsePhy(data,checksum,out,error)){++accepted;for(const auto& c:out.convexes){if(c.size()<4||c.size()>2048)return 2;for(const auto& p:c)for(float v:p)if(!std::isfinite(v)||std::abs(v)>32768.01f)return 3;}}
        else {++rejected;if(out.convexes.size()!=1||out.convexes[0][0][0]!=1)return 4;}}
    std::cout<<"PHY mutation checks passed: 20000 cases, "<<accepted<<" accepted, "<<rejected<<" rejected\n";
}
