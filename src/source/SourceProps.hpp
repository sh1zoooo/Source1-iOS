#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
namespace source1ios {
struct PreviewProp {
    std::string model;
    std::array<float,3> origin{},angles{};
    float scale=1;
    int skin=0;
};
// sprp payload after the game-lump directory; records follow gamebspfile.h.
inline bool parsePreviewProps(const std::vector<std::uint8_t>& bytes,unsigned version,int bspVersion,
                              std::vector<PreviewProp>& output){
    const unsigned strides[]={0,0,0,0,56,60,64,68,68,72};
    const unsigned stride=version<10?strides[version]:version==10?(bspVersion==21?76:72):version==11?80:0;
    if(!stride||bytes.size()>4*1024*1024)return false;
    size_t cursor=0;
    auto count=[&](int& n,int limit){if(bytes.size()-cursor<4)return false;std::memcpy(&n,bytes.data()+cursor,4);cursor+=4;return n>=0&&n<=limit;};
    int names=0;if(!count(names,1024)||size_t(names)>((bytes.size()-cursor)/128))return false;
    std::vector<std::string> dictionary;
    for(int i=0;i<names;++i){const char* p=reinterpret_cast<const char*>(bytes.data()+cursor);const auto* end=static_cast<const char*>(std::memchr(p,0,128));if(!end)return false;
        std::string name(p,end);cursor+=128;
        if(name.size()<5||name.substr(name.size()-4)!=".mdl"||name.front()=='/'||name.find("..")!=std::string::npos||name.find(':')!=std::string::npos||name.find('\\')!=std::string::npos)return false;
        dictionary.push_back(std::move(name));}
    int leaves=0;if(!count(leaves,65536)||size_t(leaves)>(bytes.size()-cursor)/2)return false;cursor+=size_t(leaves)*2;
    int props=0;if(!count(props,8192)||size_t(props)>(bytes.size()-cursor)/stride||size_t(props)*stride!=bytes.size()-cursor)return false;
    std::vector<PreviewProp> staged;staged.reserve(props);
    for(int i=0;i<props;++i,cursor+=stride){const auto* p=bytes.data()+cursor;PreviewProp prop;
        std::memcpy(prop.origin.data(),p,12);std::memcpy(prop.angles.data(),p+12,12);
        std::uint16_t model=0,first=0,length=0;std::memcpy(&model,p+24,2);std::memcpy(&first,p+26,2);std::memcpy(&length,p+28,2);std::memcpy(&prop.skin,p+32,4);
        if(model>=dictionary.size()||first>unsigned(leaves)||length>unsigned(leaves)-first||prop.skin<0)return false;
        for(float n:prop.origin)if(!std::isfinite(n)||std::abs(n)>32768)return false;
        for(float n:prop.angles)if(!std::isfinite(n)||std::abs(n)>360000)return false;
        if(version==11){std::memcpy(&prop.scale,p+76,4);if(!std::isfinite(prop.scale)||prop.scale<=0||prop.scale>16)return false;}
        prop.model=dictionary[model];staged.push_back(std::move(prop));
    }
    output=std::move(staged);return true;
}
}
