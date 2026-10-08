#include "Runtime.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <cmath>
#include "tier1/convar.h"
#include "icvar.h"
#include "tier2/tier2.h"
using namespace source1ios;
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
void frames(Runtime& r,int count){for(int i=0;i<count;++i)r.frame(1./60);}
int main(int argc,char** argv){try{
 require(argc==2,"usage: mobile_probe DOCUMENTS");Runtime r;require(r.start(std::filesystem::absolute(argv[1])),"start");
 g_pCVar->FindVar("host_timescale")->SetValue(3);
 g_pCVar->FindVar("host_framerate")->SetValue(.045f);
 require(r.startGame(),"map");frames(r,240);
 for(int fps:{30,60,120}){const float before=r.playerState().simulationTime;for(int i=0;i<fps;++i)r.frame(1./fps);
  std::cerr<<"CLOCK fps="<<fps<<" simulation delta="<<r.playerState().simulationTime-before<<" scale="<<g_pCVar->FindVar("host_timescale")->GetFloat()<<" fixed="<<g_pCVar->FindVar("host_framerate")->GetFloat()<<'\n';
  require(std::abs(r.playerState().simulationTime-before-1)<.04f,"Offline animation/simulation clock is accelerated");}
 const auto& modelTexture=r.modelTexture();std::cerr<<"GRAPHICS weapon atlas="<<modelTexture.width<<"x"<<modelTexture.height<<" bytes="<<modelTexture.pixels.size()<<'\n';
 require(modelTexture.width==4608&&modelTexture.height==512,"AWP high-detail atlas missing");
 auto resources=r.mobileResources();size_t icons=0;for(const auto& b:resources.buttons)icons+=!b.texture.pixels.empty();std::cerr<<"RESOURCES buttons="<<resources.buttons.size()<<" icons="<<icons<<" buy="<<resources.buy.size()<<" hud="<<resources.hud.size()<<'\n';
 require(resources.buttons.size()>15&&icons>15&&resources.buy.size()>15&&resources.hud.size()==4,"Imported mobile resources missing");
 auto has=[](const MobileResources& menu,const char* alias){return std::any_of(menu.buy.begin(),menu.buy.end(),[&](const auto& item){return item.alias==alias;});};
 for(const char* alias:{"glock","p228","ak47","awp","mp5navy","m3","m249","vest","vesthelm","hegrenade","flashbang","smokegrenade"})require(has(resources,alias),alias);
 require(!has(resources,"m4a1")&&!has(resources,"defuser"),"CT items in T menu");
 require(std::any_of(resources.buy.begin(),resources.buy.end(),[](const auto& item){return item.alias=="ak47"&&item.label=="AK47";}),"Radial weapon name replaced with price");
 r.playerButton(PlayerAttack2,true);frames(r,1);r.playerButton(PlayerAttack2,false);frames(r,30);require(r.playerState().fov<80,"AWP zoom missing");
 int money=r.playerState().money,price=0,team=0;require(gameBuyInfo("ak47",price,team),"AK info");require(r.buy("ak47")==0,"AK purchase");require(r.playerState().money==money-price,"Incorrect purchase debit");r.playerAction("slot1");frames(r,90);require(!strcmp(r.playerState().weapon,"weapon_ak47"),"AK not equipped");
 money=r.playerState().money;require(r.buy("ak47")==1&&r.playerState().money==money,"Repeat purchase charged");require(r.buy("m4a1")!=0&&r.playerState().money==money,"Wrong team purchase");require(r.buy("invalid_foo")!=0&&r.playerState().money==money,"Invalid purchase charged");
 require(r.buy("p228")==0,"P228 purchase");require(r.playerAction("slot2"),"Slot2");frames(r,60);require(!strcmp(r.playerState().weapon,"weapon_p228"),"P228 not equipped");require(r.playerAction("drop"),"Drop");
 require(r.playerAction("team3"),"CT switch");frames(r,120);require(r.playerState().team==3&&r.playerState().alive,"CT respawn");require(r.buy("m4a1")==0,"CT M4 purchase");
 const auto ct=r.mobileResources();for(const char* alias:{"usp","m4a1","famas","awp","defuser","vest","vesthelm","hegrenade"})require(has(ct,alias),alias);
 require(!has(ct,"ak47")&&!has(ct,"galil"),"T items in CT menu");
 require(r.playerAction("team2"),"T switch");frames(r,120);require(r.playerState().team==2&&r.playerState().alive,"T respawn");r.clearInput();r.stopGame();r.stop();std::cerr<<"MOBILE PASS\n";
 }catch(const std::exception& e){std::cerr<<"MOBILE FAIL: "<<e.what()<<'\n';return 1;}}
