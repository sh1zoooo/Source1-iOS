#include "Runtime.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
using namespace source1ios;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void step(Runtime& runtime,double seconds){for(int i=0;i<int(seconds*60);++i)runtime.frame(1./60);}
int main(int argc,char** argv){try{
 require(argc==2,"usage: world_probe DOCUMENTS");Runtime r;require(r.start(std::filesystem::absolute(argv[1])),"startup");require(r.startGame("awp_lego_2"),"map");step(r,4);
 auto resources=r.mobileResources();size_t icons=0;for(const auto& b:resources.buttons)icons+=!b.texture.pixels.empty();std::cerr<<"APK resources config="<<resources.config<<" buttons="<<resources.buttons.size()<<" icons="<<icons<<" hud="<<resources.hud.size()<<"\n";require(resources.hud.size()==4,"HUD defaults");require(icons>20,"APK touch icons");
 const auto original=r.vertices(1.8);const auto before=original.size();size_t originalWorld=0;for(const auto& v:original)originalWorld+=v.material[0]>=0;require(r.playerAction("drop"),"drop command");step(r,.2);
 bool dropped=false;for(int i=0;i<r.playerState().worldCount;++i)dropped|=r.playerState().world[i].kind==0;
 require(dropped,"dropped weapon server entity");auto geometry=r.vertices(1.8);size_t worldVertices=0;
 for(const auto& v:geometry)worldVertices+=v.material[0]>=0;
 std::cerr<<"Dropped entities="<<r.playerState().worldCount<<" geometry="<<geometry.size()<<" before="<<before<<" world="<<worldVertices<<"\n";
 require(worldVertices>originalWorld,"dropped weapon rendered");
 // Kill/restart returns to the real buy zone and resets all pending inputs.
 r.stopGame();require(r.startGame("awp_lego_2"),"restart");step(r,4);
 for(const char* alias:{"hegrenade","flashbang","smokegrenade"}){
  const int result=r.buy(alias);std::cerr<<"Buy "<<alias<<" result="<<result<<"\n";require(result==0,"grenade buy");
 }
 for(int grenade=0;grenade<3;++grenade){
  require(r.playerAction("slot4"),"grenade slot");step(r,1);
  std::cerr<<"Selected "<<r.playerState().weapon<<" cycle="<<r.playerState().viewCycle<<"\n";
  require(std::strstr(r.playerState().weapon,"grenade")||std::strstr(r.playerState().weapon,"flashbang"),"grenade selected");
  // A tap shorter than one server tick must survive until usercmd dispatch.
  r.playerButton(PlayerAttack,true);r.playerButton(PlayerAttack,false);
  bool projectile=false;
  for(int frame=0;frame<180;++frame){r.frame(1./60);if(frame%30==0)std::cerr<<"Grenade frame "<<frame<<" weapon="<<r.playerState().weapon<<" reserve="<<r.playerState().reserve<<" world="<<r.playerState().worldCount<<" anim="<<r.playerState().viewAnimation<<" cycle="<<r.playerState().viewCycle<<"\n";for(int i=0;i<r.playerState().worldCount;++i)projectile|=r.playerState().world[i].kind==1;}
  require(projectile,"grenade projectile from short tap");step(r,5);
 }
 bool smoke=false;for(int i=0;i<r.playerState().worldCount;++i)smoke|=r.playerState().world[i].kind==2;require(smoke,"live smoke effect entity");bool alpha=false;for(const auto& v:r.vertices(1.8))alpha|=v.color[3]<1;require(alpha,"smoke billboard geometry");
 r.stopGame();r.stop();std::cerr<<"WORLD GAMEPLAY PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"WORLD FAIL: "<<e.what()<<"\n";return 1;}}
