#pragma once
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace source1ios {
struct PreviewSpawn { std::array<float,3> origin{}, angles{}; std::string classname; };
// Data-only BSP entity reader. No entity factories, outputs or configs run.
inline bool parsePreviewSpawns(std::string text,std::vector<PreviewSpawn>& output) {
    if(text.size()>1024*1024)return false;
    if(!text.empty()&&text.back()=='\0')text.pop_back();
    if(text.find('\0')!=std::string::npos)return false;
    size_t cursor=0;bool failed=false;
    auto space=[&]{while(cursor<text.size()){
        if(static_cast<unsigned char>(text[cursor])<=32){++cursor;continue;}
        if(text[cursor]=='/'&&cursor+1<text.size()&&text[cursor+1]=='/'){
            cursor+=2;while(cursor<text.size()&&text[cursor]!='\n')++cursor;continue;
        }break;}};
    auto token=[&](std::string& value)->bool{
        space();value.clear();if(cursor==text.size())return false;
        const char first=text[cursor++];
        if(first=='{'||first=='}'){value=first;return true;}
        const bool quoted=first=='"';if(!quoted)value+=first;
        while(cursor<text.size()){
            char c=text[cursor];
            if(quoted){++cursor;if(c=='"')return true;
                if(c=='\\'&&cursor<text.size()&&(text[cursor]=='"'||text[cursor]=='\\'))c=text[cursor++];
            }else{if(static_cast<unsigned char>(c)<=32||c=='{'||c=='}')return true;++cursor;}
            value+=c;if(value.size()>4096){failed=true;return false;}
        }
        if(quoted){failed=true;return false;}return true;
    };
    auto numbers=[](const std::string& value,std::array<float,3>& result,unsigned count,float limit){
        const char* p=value.c_str();for(unsigned i=0;i<count;++i){char* end=nullptr;
            const float n=std::strtof(p,&end);if(end==p||!std::isfinite(n)||std::abs(n)>limit)return false;
            result[i]=n;p=end;
            if(i+1<count&&(!*p||static_cast<unsigned char>(*p)>32))return false;
        }while(*p&&static_cast<unsigned char>(*p)<=32)++p;return !*p;
    };
    std::vector<PreviewSpawn> staged;std::string key,value;unsigned entities=0;
    while(token(key)){
        if(key!="{"||++entities>8192)return false;
        std::string classname,origin,angles,angle;unsigned pairs=0;bool closed=false;
        while(token(key)){
            if(key=="}"){closed=true;break;}
            if(key=="{"||++pairs>256||!token(value)||value=="{"||value=="}")return false;
            if(key=="classname")classname=value;else if(key=="origin")origin=value;
            else if(key=="angles")angles=value;else if(key=="angle")angle=value;
        }
        if(!closed)return false;
        if(classname!="info_player_start"&&classname!="info_player_counterterrorist"&&
           classname!="info_player_terrorist"&&classname!="info_player_deathmatch")continue;
        PreviewSpawn spawn;spawn.classname=classname;
        if(!numbers(origin,spawn.origin,3,32768))return false;
        if(!angles.empty()){if(!numbers(angles,spawn.angles,3,360000))return false;}
        else if(!angle.empty()){
            std::array<float,3> yaw{};if(!numbers(angle,yaw,1,360000))return false;
            if(yaw[0]==-1)spawn.angles[0]=-90;else if(yaw[0]==-2)spawn.angles[0]=90;
            else spawn.angles[1]=yaw[0];
        }
        if(staged.size()>=512)return false;staged.push_back(std::move(spawn));
    }
    if(failed)return false;
    output=std::move(staged);return true;
}
}
