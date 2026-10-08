#include "SourcePlayer.hpp"
#include <algorithm>
#include <cmath>
#ifdef SOURCE_GAME_LINK
#include "tier0/platform.h"
#include "filesystem.h"
#include "host_cmd.h"
#include "host_state.h"
#include "server.h"
#include "eiface.h"
#include "tier1/interface.h"
#include "tier1/convar.h"
#include "tier2/tier2.h"
#include "icvar.h"
#include "cmd.h"
#include "game/server/iplayerinfo.h"
#include "game/shared/in_buttons.h"
extern CGameServer sv;
extern IServerGameDLL* serverGameDLL;
#endif
namespace source1ios {
#ifndef SOURCE_GAME_LINK
bool gameBuyInfo(const char*,int&,int&) { return false; }
#endif
bool SourcePlayer::start(const std::string& map) {
    stop();error_.clear();
#ifdef SOURCE_GAME_LINK
    if(map.empty()||map.size()>64||map.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos){error_="Invalid map name";return false;}
    if(!serverGameDLL||!sv.dll_initialized||!g_pFullFileSystem->FileExists(("maps/"+map+".bsp").c_str(),"GAME")){
        error_="Import ClientMod cm/cstrike/hl2 resources in Files, then load a map";return false;
    }
    auto factory=Sys_GetFactoryThis();
    auto* bots=static_cast<IBotManager*>(factory(INTERFACEVERSION_PLAYERBOTMANAGER,nullptr));
    if(!bots){error_="Original player-control interface missing";return false;}
    for(const auto& setting:{std::pair<const char*,int>{"mp_freezetime",0},{"mp_autoteambalance",0},{"mp_limitteams",0},{"mp_startmoney",16000},{"mp_buytime",99},{"bot_quota",0},{"sv_hibernate_when_empty",0}})
        if(auto* var=g_pCVar->FindVar(setting.first))var->SetValue(setting.second);
    auto name=map;ownsLevel_=true;
    if(!Host_NewGame(name.data(),false,false)||!sv.IsActive()){error_="Original GameDLL could not activate map";stop();return false;}
    // UIKit owns offline practice commands; Android menu/autoexec commands must
    // not change maps underneath the controlled player and its camera.
    Cbuf_Init();
    entity_=bots->CreateBot("iOS local player");
    auto* opponent=bots->CreateBot("iOS practice opponent");opponent_=opponent;
    if(!entity_||!opponent||!gamePlayerSpawn(entity_,2)||!gamePlayerSpawn(opponent,3)){
        error_="Original CCSPlayer round spawn failed";stop();return false;
    }
    controller_=bots->GetBotController(static_cast<edict_t*>(entity_));
    if(!controller_||!gamePlayerRead(entity_,state_)){error_="Original player state unavailable";stop();return false;}
    spawnCount_=sv.GetSpawnCount();lastTick_=sv.GetTick();command_=0;
    yaw_=state_.angles[1];pitch_=state_.angles[0];
    Msg("Source player practice active: CCSPlayer, real usercmd movement, health/armor/ammo; map %s\n",map.c_str());
    return true;
#else
    error_="Linked CS:S GameDLL required";return false;
#endif
}
int SourcePlayer::buy(const std::string& alias){
#ifdef SOURCE_GAME_LINK
    if(!active()||alias.empty()||alias.size()>32||alias.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)return 5;
    const auto result=gamePlayerBuy(entity_,alias.c_str());gamePlayerRead(entity_,state_);return result;
#else
    return 5;
#endif
}
bool SourcePlayer::action(const std::string& action){
#ifdef SOURCE_GAME_LINK
    if(!active())return false;
    if(action=="team2"||action=="team3"){
        const int team=action.back()-'0';buttons_=0;forward_=right_=0;
        if(!gamePlayerSpawn(opponent_,team==2?3:2)||!gamePlayerSpawn(entity_,team))return false;
        gamePlayerRead(entity_,state_);yaw_=state_.angles[1];pitch_=state_.angles[0];return true;
    }
    const bool result=gamePlayerAction(entity_,action.c_str());gamePlayerRead(entity_,state_);return result;
#else
    return false;
#endif
}
void SourcePlayer::stop() {
    entity_=opponent_=controller_=nullptr;state_={};forward_=right_=0;buttons_=0;
#ifdef SOURCE_GAME_LINK
    if(ownsLevel_&&serverGameDLL){
        HostState_GameShutdown();
        for(unsigned i=0;i<3&&(sv.IsActive()||HostState_IsGameShuttingDown());++i)HostState_Frame(0);
    }
#endif
    ownsLevel_=false;
}
float SourcePlayer::tickInterval() const {
#ifdef SOURCE_GAME_LINK
    return serverGameDLL?serverGameDLL->GetTickInterval():.015f;
#else
    return .015f;
#endif
}
void SourcePlayer::move(float forward,float right) {
    if(!std::isfinite(forward)||!std::isfinite(right))return;
    forward_=std::clamp(forward,-1.f,1.f);right_=std::clamp(right,-1.f,1.f);
}
void SourcePlayer::look(float yaw,float pitch) {
    if(!std::isfinite(yaw)||!std::isfinite(pitch))return;
    yaw_=std::remainder(yaw_+yaw,360.f);pitch_=std::clamp(pitch_+pitch,-89.f,89.f);
}
void SourcePlayer::button(unsigned flag,bool pressed) {
    flag&=PlayerJump|PlayerDuck|PlayerAttack|PlayerReload|PlayerAttack2|PlayerUse;
    if(pressed)buttons_|=flag;else buttons_&=~flag;
}
void SourcePlayer::step() {
#ifdef SOURCE_GAME_LINK
    if(!active())return;
    if(!sv.IsActive()||sv.GetSpawnCount()!=spawnCount_){entity_=opponent_=controller_=nullptr;state_={};error_="Game level changed";return;}
    if(sv.GetTick()==lastTick_)return;
    lastTick_=sv.GetTick();
    CBotCmd command;command.command_number=++command_;command.tick_count=lastTick_;
    command.viewangles=QAngle(pitch_,yaw_,0);
    const float length=std::max(1.f,std::sqrt(forward_*forward_+right_*right_));
    command.forwardmove=forward_*400/length;command.sidemove=right_*400/length;
    if(buttons_&PlayerJump)command.buttons|=IN_JUMP;
    if(buttons_&PlayerDuck)command.buttons|=IN_DUCK;
    if(buttons_&PlayerAttack)command.buttons|=IN_ATTACK;
    if(buttons_&PlayerReload)command.buttons|=IN_RELOAD;
    if(buttons_&PlayerAttack2)command.buttons|=IN_ATTACK2;
    if(buttons_&PlayerUse)command.buttons|=IN_USE;
    static_cast<IBotController*>(controller_)->RunPlayerMove(&command);
    gamePlayerRead(entity_,state_);
#endif
}
}
