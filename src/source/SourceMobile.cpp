#include "SourceMobile.hpp"
#include "SourceMap.hpp"
#include "PlayerState.hpp"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "tier1/convar.h"
#include "tier1/KeyValues.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
namespace source1ios {
namespace {
bool text(const char* path,std::string& out){auto f=g_pFullFileSystem->Open(path,"rb","GAME");if(!f)return false;const auto size=g_pFullFileSystem->Size(f);if(size>256*1024){g_pFullFileSystem->Close(f);return false;}out.resize(size);bool ok=g_pFullFileSystem->Read(out.data(),size,f)==int(size);g_pFullFileSystem->Close(f);return ok;}
void buyNodes(KeyValues* node,MobileResources& out,const char* category,std::set<std::string>& seen,int team,int depth=0){
 if(!node||depth>16||out.buy.size()>64)return;
 for(auto* child=node->GetFirstSubKey();child;child=child->GetNextKey()){
  CCommand command;command.Tokenize(child->GetString("command",""));
  if(command.ArgC()==2&&!strcmp(command[0],"buy")){
   std::string alias=command[1];int price=0,restricted=0;
   if(alias.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")==std::string::npos&&gameBuyInfo(alias.c_str(),price,restricted)&&(restricted==0||restricted==team)&&seen.insert(alias).second){
    std::string label=child->GetString("labelText",alias.c_str());if(label.rfind("#Cstrike_",0)==0)label=label.substr(9);out.buy.push_back({alias,label,category,price});
   }
  }
  buyNodes(child,out,category,seen,team,depth+1);
 }
}
}
MobileResources loadMobileResources(int team){
 MobileResources out;std::string data;
 for(const auto* path:{"cfg/touch.cfg","cfg/touch_default.cfg"})if(text(path,data)){out.config=path;break;}
 std::istringstream lines(data);std::string line;
 while(std::getline(lines,line)&&out.buttons.size()<64){CCommand args;args.Tokenize(line.c_str());
  if(args.ArgC()==2){float* setting=nullptr;std::string key=args[0];if(key=="touch_yaw")setting=&out.yaw;else if(key=="touch_pitch")setting=&out.pitch;else if(key=="touch_forwardzone")setting=&out.forwardZone;else if(key=="touch_sidezone")setting=&out.sideZone;
   if(setting){char* end=nullptr;float value=strtof(args[1],&end);bool zone=key.find("zone")!=std::string::npos;if(end&&!*end&&std::isfinite(value)&&value>=(zone?.01f:1.f)&&value<=(zone?1.f:1000.f))*setting=value;}continue;
  }
  if(args.ArgC()<12||strcmp(args[0],"touch_addbutton"))continue;
  MobileButton b;b.name=args[1];b.icon=args[2];b.command=args[3];float* values[]={&b.x1,&b.y1,&b.x2,&b.y2};bool valid=true;
  for(int i=0;i<4;++i){char* end=nullptr;*values[i]=strtof(args[4+i],&end);valid&=end&&!*end&&std::isfinite(*values[i])&&*values[i]>=-.5f&&*values[i]<=1.5f;}
  if(!valid||b.x2<=b.x1||b.y2<=b.y1||b.command.empty())continue;
  for(int i=0;i<4;++i)b.color[i]=std::clamp(atoi(args[8+i]),0,255);
  if(!b.icon.empty()&&b.icon.size()<128&&b.icon.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-/")==std::string::npos&&b.icon.find("//")==std::string::npos)sourceDecodeUITexture(b.icon,b.texture);
  // Xash replaces duplicate names; keep that behavior for imported touch.cfg.
  auto prior=std::find_if(out.buttons.begin(),out.buttons.end(),[&](const auto& other){return other.name==b.name;});if(prior!=out.buttons.end())*prior=std::move(b);else out.buttons.push_back(std::move(b));
 }
 std::string hud;if(text("scripts/hudlayout.res",hud)&&hud.find("#include")==std::string::npos&&hud.find("#base")==std::string::npos){
  auto* kv=new KeyValues("hud");if(kv->LoadFromBuffer("hudlayout",hud.c_str()))for(const char* name:{"HudHealth","HudArmor","HudAmmo","HudAccount"}){
   if(auto* panel=kv->FindKey(name)){float width=panel->GetFloat("wide",0),height=panel->GetFloat("tall",0);if(width>0&&height>0&&width<640&&height<480)out.hud.push_back({name,panel->GetString("xpos","0"),panel->GetString("ypos","0"),width,height});}
  }kv->deleteThis();
 }
 const bool ct=team==3;std::set<std::string> seen;
 for(const auto& entry:{std::pair<const char*,std::string>{"Pistols",ct?"buypistols_ct":"buypistols_ter"},{"Rifles",ct?"buyrifles_ct":"buyrifles_ter"},{"SMG",ct?"buysubmachineguns_ct":"buysubmachineguns_ter"},{"Shotguns","buyshotguns"},{"Machine guns","buymachineguns"},{"Equipment",ct?"buyequipment_ct":"buyequipment_ter"}}){
  const auto path="resource/ui/"+entry.second+".res";std::string res;
  if(!text(path.c_str(),res)||res.find("#include")!=std::string::npos||res.find("#base")!=std::string::npos)continue;
  auto* kv=new KeyValues("buy");if(kv->LoadFromBuffer(path.c_str(),res.c_str()))buyNodes(kv,out,entry.first,seen,team);kv->deleteThis();
 }
 return out;
}
}
