#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace source1ios {
// Extract terminal convex point clouds only; imported pointers/topology never
// enter legacy VCollideLoad / UnserializeCollide.
struct PhyGeometry {std::vector<std::vector<std::array<float,3>>> convexes;};
namespace phy_detail {
inline bool range(size_t p,size_t n,size_t limit){return p<=limit&&n<=limit-p;}
template<class T> inline T get(const std::vector<std::uint8_t>& b,size_t p){T n;std::memcpy(&n,b.data()+p,sizeof(n));return n;}
inline std::uint32_t id(char a,char b,char c,char d){return std::uint32_t(a)|(std::uint32_t(b)<<8)|(std::uint32_t(c)<<16)|(std::uint32_t(d)<<24);}
inline bool vec(const std::vector<std::uint8_t>& b,size_t p,float limit){for(unsigned i=0;i<3;++i){float n=get<float>(b,p+i*4);if(!std::isfinite(n)||std::abs(n)>limit)return false;}return true;}
inline bool ledge(const std::vector<std::uint8_t>& b,size_t base,size_t bytes,size_t p,PhyGeometry& out,std::string& error){
    auto fail=[&](const char* why){error=why;return false;};
    if(p%16||!range(p,16,bytes))return fail("PHY ledge header outside surface or unaligned");
    const auto flags=get<std::uint32_t>(b,base+p+8);const auto triangles=get<std::int16_t>(b,base+p+12);
    const unsigned storage=(flags>>2)&3,units=flags>>8;
    if((flags&3)!=0||storage>1||triangles<4||triangles>4096||units<unsigned(triangles)+1||!range(p,size_t(units)*16,bytes))return fail("unsupported PHY ledge flags/counts");
    const std::int64_t points=std::int64_t(p)+get<std::int32_t>(b,base+p);
    if(points<48||points%16||points>=std::int64_t(bytes))return fail("PHY point array outside surface");
    std::unordered_set<unsigned> used;std::vector<std::array<float,3>> cloud;
    const size_t begin=p+16,end=begin+size_t(triangles)*16;
    for(int t=0;t<triangles;++t){const size_t tri=begin+size_t(t)*16;const auto meta=get<std::uint32_t>(b,base+tri);
        if((meta&0xfff)!=unsigned(t)||((meta>>12)&0xfff)>=unsigned(triangles))return fail("PHY triangle indices invalid");
        for(unsigned e=0;e<3;++e){const size_t edge=tri+4+e*4;const auto word=get<std::uint32_t>(b,base+edge);const unsigned index=word&0xffff;
            int opposite=(word>>16)&0x7fff;if(opposite&0x4000)opposite-=0x8000;
            const std::int64_t target=std::int64_t(edge)+std::int64_t(opposite)*4;
            if(!opposite||target<std::int64_t(begin)||target>=std::int64_t(end)||(target-p)%16==0)return fail("PHY opposite edge outside triangles");
            const auto back=get<std::uint32_t>(b,base+size_t(target));int reverse=(back>>16)&0x7fff;if(reverse&0x4000)reverse-=0x8000;
            if(target+std::int64_t(reverse)*4!=std::int64_t(edge))return fail("PHY edges are not reciprocal");
            const size_t at=size_t(points)+size_t(index)*16;
            if(!range(at,16,bytes)||!vec(b,base+at,832.3072f))return fail("PHY point outside surface or coordinate bounds");
            if(used.insert(index).second){if(cloud.size()>=2048)return fail("PHY convex point budget exceeded");
                cloud.push_back({get<float>(b,base+at)/.0254f,get<float>(b,base+at+8)/.0254f,-get<float>(b,base+at+4)/.0254f});}}
    }
    if(cloud.size()<4||out.convexes.size()>=128)return fail("PHY convex count outside budget");
    out.convexes.push_back(std::move(cloud));return true;
}
inline bool surface(const std::vector<std::uint8_t>& b,size_t base,size_t bytes,PhyGeometry& out,std::string& error){
    auto fail=[&](const char* why){error=why;return false;};if(bytes<48||!range(base,bytes,b.size()))return fail("PHY compact surface truncated");
    const auto packed=get<std::uint32_t>(b,base+28);const auto root=get<std::int32_t>(b,base+32);const auto signature=get<std::uint32_t>(b,base+44);
    if((packed>>8)!=bytes||root<48||root%4||!range(size_t(root),28,bytes)||signature!=id('I','V','P','S'))return fail("PHY compact surface size/root/signature invalid");
    if(!vec(b,base,832.3072f)||!vec(b,base+12,1000000))return fail("PHY surface vectors invalid");
    const float radius=get<float>(b,base+24);if(!std::isfinite(radius)||radius<=0||radius>1000000)return fail("PHY surface radius invalid");
    std::vector<std::pair<size_t,unsigned>> pending{{size_t(root),0}};std::unordered_set<size_t> seen,ledges;
    while(!pending.empty()){auto item=pending.back();pending.pop_back();const size_t p=item.first;
        if(item.second>64||seen.size()>=4096||!range(p,28,bytes)||!seen.insert(p).second)return fail("PHY ledgetree cycle/depth/range invalid");
        const int right=get<std::int32_t>(b,base+p),offset=get<std::int32_t>(b,base+p+4);if(!vec(b,base+p+8,832.3072f))return fail("PHY node center invalid");
        const float r=get<float>(b,base+p+20);if(!std::isfinite(r)||r<0||r>1000000)return fail("PHY node radius invalid");
        if(right){if(right<56||right%4||!range(p+size_t(right),28,bytes)||!range(p+28,28,bytes))return fail("PHY node child range invalid");pending.push_back({p+28,item.second+1});pending.push_back({p+size_t(right),item.second+1});}
        else {const std::int64_t at=std::int64_t(p)+offset;if(!offset||at<48||at>=std::int64_t(bytes)||!ledges.insert(size_t(at)).second)return fail("PHY terminal ledge offset invalid");if(!ledge(b,base,bytes,size_t(at),out,error))return false;}
    }return true;
}
inline bool solid(const std::vector<std::uint8_t>& b,size_t p,size_t n,PhyGeometry& out,std::string& error){
    if(n<48||!range(p,n,b.size())){error="PHY solid range invalid";return false;}
    if(get<std::uint32_t>(b,p)==id('V','P','H','Y')){
        if(get<std::uint16_t>(b,p+4)!=0x100||get<std::uint16_t>(b,p+6)!=0||get<std::int32_t>(b,p+8)!=std::int64_t(n)-28||get<std::int32_t>(b,p+24)!=0||!vec(b,p+12,1000000)){error="unsupported VPHY envelope";return false;}
        p+=28;n-=28;}
    return surface(b,p,n,out,error);
}
}
inline bool parsePhy(const std::vector<std::uint8_t>& bytes,std::int32_t checksum,PhyGeometry& output,std::string& error){
    auto fail=[&](const char* why){error=why;return false;};if(bytes.size()<16||bytes.size()>16*1024*1024)return fail("PHY file size outside bounds");
    const int count=phy_detail::get<std::int32_t>(bytes,8);
    if(phy_detail::get<int>(bytes,0)!=16||phy_detail::get<int>(bytes,4)!=0||count!=1||phy_detail::get<std::int32_t>(bytes,12)!=checksum)return fail("PHY header/count/MDL checksum mismatch (one static solid required)");
    PhyGeometry result;size_t cursor=16;
    for(int i=0;i<count;++i){if(!phy_detail::range(cursor,4,bytes.size()))return fail("PHY solid length missing");const auto n=phy_detail::get<std::int32_t>(bytes,cursor);cursor+=4;
        if(n<48||n>4*1024*1024||!phy_detail::range(cursor,size_t(n),bytes.size()))return fail("PHY solid length outside file");if(!phy_detail::solid(bytes,cursor,size_t(n),result,error))return false;cursor+=size_t(n);}
    if(result.convexes.empty()||bytes.size()-cursor>1024*1024)return fail("PHY empty geometry or keydata limit");
    output=std::move(result);error.clear();return true;
}
}
