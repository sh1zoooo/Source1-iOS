#include "Runtime.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
using namespace source1ios;
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
void frames(Runtime& r,int count){for(int i=0;i<count;++i)r.frame(1./60);}
int main(int argc,char** argv){try{
 require(argc==2,"usage: mobile_probe DOCUMENTS");Runtime r;require(r.start(std::filesystem::absolute(argv[1])),"start");require(r.startGame(),"map");frames(r,240);
 const auto& modelTexture=r.modelTexture();std::cerr<<"GRAPHICS weapon atlas="<<modelTexture.width<<"x"<<modelTexture.height<<" bytes="<<modelTexture.pixels.size()<<'\n';
 require(modelTexture.width==4608&&modelTexture.height==512,"AWP high-detail atlas missing");
 auto resources=r.mobileResources();size_t icons=0;for(const auto& b:resources.buttons)icons+=!b.texture.pixels.empty();std::cerr<<"RESOURCES buttons="<<resources.buttons.size()<<" icons="<<icons<<" buy="<<resources.buy.size()<<" hud="<<resources.hud.size()<<'\n';
 require(resources.buttons.size()>15&&icons>15&&resources.buy.size()>15&&resources.hud.size()==4,"Imported mobile resources missing");
 r.playerButton(PlayerAttack2,true);frames(r,1);r.playerButton(PlayerAttack2,false);frames(r,30);require(r.playerState().fov<80,"AWP zoom missing");
 int money=r.playerState().money,price=0,team=0;require(gameBuyInfo("ak47",price,team),"AK info");require(r.buy("ak47")==0,"AK purchase");require(r.playerState().money==money-price,"Incorrect purchase debit");r.playerAction("slot1");frames(r,90);require(!strcmp(r.playerState().weapon,"weapon_ak47"),"AK not equipped");
 money=r.playerState().money;require(r.buy("ak47")==1&&r.playerState().money==money,"Repeat purchase charged");require(r.buy("m4a1")!=0&&r.playerState().money==money,"Wrong team purchase");require(r.buy("invalid_foo")!=0&&r.playerState().money==money,"Invalid purchase charged");
 require(r.buy("p228")==0,"P228 purchase");require(r.playerAction("slot2"),"Slot2");frames(r,60);require(!strcmp(r.playerState().weapon,"weapon_p228"),"P228 not equipped");require(r.playerAction("drop"),"Drop");
 require(r.playerAction("team3"),"CT switch");frames(r,120);require(r.playerState().team==3&&r.playerState().alive,"CT respawn");require(r.buy("m4a1")==0,"CT M4 purchase");
 require(r.playerAction("team2"),"T switch");frames(r,120);require(r.playerState().team==2&&r.playerState().alive,"T respawn");r.clearInput();r.stopGame();r.stop();std::cerr<<"MOBILE PASS\n";
 }catch(const std::exception& e){std::cerr<<"MOBILE FAIL: "<<e.what()<<'\n';return 1;}}
