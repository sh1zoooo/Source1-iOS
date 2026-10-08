#include "PlayerState.hpp"
#include "cbase.h"
#include "cs_player.h"
#include "weapon_csbase.h"
#include "cs_shareddefs.h"
#include "cs_gamerules.h"
#include "baseviewmodel_shared.h"
#include "tier1/strtools.h"
#include <algorithm>
#include <vector>

namespace source1ios {
namespace {
CCSPlayer* player(void* entity) {
    auto* edict=static_cast<edict_t*>(entity);
    return edict && !edict->IsFree() ? dynamic_cast<CCSPlayer*>(CBaseEntity::Instance(edict)) : nullptr;
}
void copy(float* out,const Vector& in){for(int i=0;i<3;++i)out[i]=in[i];}
void bones(CBaseAnimating* entity,float out[128][12],int& count,bool local){
    if(!entity)return;auto* header=entity->GetModelPtr();
    if(!header||header->numbones()<1||header->numbones()>128)return;
    matrix3x4_t pose[MAXSTUDIOBONES];
    for(int bone=0;bone<header->numbones();++bone)SetIdentityMatrix(pose[bone]);
    entity->SetupBones(pose,BONE_USED_BY_ANYTHING);
    matrix3x4_t inverse;if(local)MatrixInvert(entity->EntityToWorldTransform(),inverse);
    for(int bone=0;bone<header->numbones();++bone){matrix3x4_t transform;
        if(!(header->pBone(bone)->flags&BONE_USED_BY_VERTEX_AT_LOD(0)))SetIdentityMatrix(pose[bone]);
        if(local)ConcatTransforms(inverse,pose[bone],transform);else MatrixCopy(pose[bone],transform);
        for(int row=0;row<3;++row)for(int col=0;col<4;++col){
            if(!IsFinite(transform[row][col])){static bool logged=false;if(!logged){Msg("Source player bone pose rejected: %s bone %d; nonfinite matrix\n",STRING(entity->GetModelName()),bone);logged=true;}return;}out[bone][row*4+col]=transform[row][col];
        }
    }
    count=header->numbones();
}
void animation(CBaseAnimating* entity,char* name,int size,float& cycle,int& skin){
    if(!entity)return;cycle=entity->GetCycle();skin=entity->m_nSkin.Get();
    auto* header=entity->GetModelPtr();const int sequence=entity->GetSequence();
    if(!header||sequence<0||sequence>=header->GetNumSeq())return;
    const int index=header->iRelativeAnim(sequence,header->pSeqdesc(sequence).anim(0,0));
    if(index<0||(!header->IsVirtual()&&index>=header->GetRenderHdr()->numlocalanim))return;
    V_strncpy(name,header->pAnimdesc(index).pszName(),size);
}
}
bool gameBuyInfo(const char* alias,int& price,int& team){
    auto* info=GetWeaponInfo(AliasToWeaponID(GetTranslatedWeaponAlias(alias)));if(!info)return false;
    price=info->GetWeaponPrice();team=info->m_iTeam;return price>=0;
}
int gamePlayerBuy(void* entity,const char* alias){
    auto* p=player(entity);if(!p||!p->IsAlive())return BUY_PLAYER_CANT_BUY;
    // Local practice explicitly permits shopping away from map buy triggers;
    // prices, team restrictions, inventory and account deduction stay original.
    const bool zone=p->m_bInBuyZone;p->m_bInBuyZone=true;
    const auto result=p->HandleCommand_Buy(alias);p->m_bInBuyZone=zone;return result;
}
bool gamePlayerAction(void* entity,const char* action){
    auto* p=player(entity);if(!p||!p->IsAlive())return false;
    if(!V_strcmp(action,"inspect")){
        auto* vm=p->GetViewModel();auto* weapon=p->GetActiveCSWeapon();if(!vm||!weapon)return false;
        for(const char* name:{"lookat01","inspect","fidget"}){const int sequence=vm->LookupSequence(name);if(sequence>=0){vm->SendViewModelMatchingSequence(sequence);weapon->SetWeaponIdleTime(gpGlobals->curtime+vm->SequenceDuration());return true;}}return false;
    }
    if(!V_strcmp(action,"invnext")||!V_strcmp(action,"invprev")){
        std::vector<CBaseCombatWeapon*> weapons;for(int i=0;i<p->WeaponCount();++i)if(auto* weapon=p->GetWeapon(i))weapons.push_back(weapon);
        std::sort(weapons.begin(),weapons.end(),[](const CBaseCombatWeapon* a,const CBaseCombatWeapon* b){return a->GetSlot()!=b->GetSlot()?a->GetSlot()<b->GetSlot():a->GetPosition()<b->GetPosition();});
        if(weapons.empty())return false;const auto found=std::find(weapons.begin(),weapons.end(),p->GetActiveWeapon());
        int index=found==weapons.end()?0:int(found-weapons.begin());const int direction=!V_strcmp(action,"invnext")?1:-1;
        for(size_t attempt=0;attempt<weapons.size();++attempt){index=(index+direction+int(weapons.size()))%int(weapons.size());if(p->Weapon_Switch(weapons[index]))return true;}return false;
    }
    if(!V_strcmp(action,"lastinv")){p->SelectLastItem();return true;}
    if(V_strlen(action)==5&&!V_strncmp(action,"slot",4)&&action[4]>='1'&&action[4]<='5'){
        const int slot=action[4]-'1';auto* weapon=p->Weapon_GetSlot(slot);
        if(slot==3){auto* active=p->GetActiveWeapon();bool passed=false;CBaseCombatWeapon* first=nullptr;
            for(int i=0;i<p->WeaponCount();++i){auto* candidate=p->GetWeapon(i);if(!candidate||candidate->GetSlot()!=slot)continue;if(!first)first=candidate;
                if(passed){weapon=candidate;break;}if(candidate==active)passed=true;}if(passed&&weapon==active)weapon=first;}
        return weapon&&p->Weapon_Switch(weapon);
    }
    if(!V_strcmp(action,"drop")){CCommand args;args.Tokenize("drop");return p->ClientCommand(args);}
    return false;
}
bool gamePlayerSpawn(void* entity,int team) {
    auto* p=player(entity);if(!p||!p->IsBot()||(team!=TEAM_TERRORIST&&team!=TEAM_CT))return false;
    p->ChangeTeam(team);
    p->HandleCommand_JoinClass(0);
    // Offline practice uses the original round-respawn path, including inventory,
    // player model, VPhysics hull and the active player state.
    p->RoundRespawn();
    // Practice explicitly respawns its controlled players; do not schedule the
    // match constructor's second map-entity rebuild after this fresh level spawn.
    if(auto* rules=CSGameRules()){rules->m_flRestartRoundTime=0;rules->m_bCompleteReset=false;}
    if(p->m_iAccount<16000)p->AddAccount(16000-p->m_iAccount,false);
    return p->IsAlive()&&!p->IsObserver();
}
void gamePlayerAdvanceView(void* entity,float seconds){
    auto* p=player(entity);if(p&&p->GetViewModel()&&seconds>0&&seconds<.1f)p->GetViewModel()->StudioFrameAdvanceManual(seconds);
}
bool gamePlayerRead(void* entity,PlayerState& out) {
    auto* p=player(entity);out={};if(!p)return false;
    out.active=true;out.alive=p->IsAlive();out.grounded=(p->GetFlags()&FL_ONGROUND)!=0;
    out.crouched=(p->GetFlags()&FL_DUCKING)!=0;out.health=p->GetHealth();out.armor=p->ArmorValue();
    out.fov=p->GetFOV();out.simulationTime=gpGlobals->curtime;out.money=p->m_iAccount;out.team=p->GetTeamNumber();out.tick=gpGlobals->tickcount;
    copy(out.origin,p->GetAbsOrigin());copy(out.eye,p->EyePosition());copy(out.velocity,p->GetAbsVelocity());
    const auto angles=p->EyeAngles();const auto punch=p->GetPunchAngle();
    for(int i=0;i<3;++i){out.angles[i]=angles[i];out.punch[i]=punch[i];}
    const float remaining=p->m_blindStartTime+p->m_flFlashDuration-gpGlobals->curtime;
    if(remaining>0&&p->m_flFlashDuration>0)out.flashAlpha=clamp(remaining/p->m_flFlashDuration,0.f,1.f)*p->m_flFlashMaxAlpha/255.f;
    for(auto* e=gEntList.FirstEnt();e&&out.worldCount<128;e=gEntList.NextEnt(e)){
        const char* name=e->GetClassname();const bool smoke=!V_strcmp(name,"env_particlesmokegrenade");
        auto* weapon=dynamic_cast<CBaseCombatWeapon*>(e);
        const bool projectile=V_strstr(name,"grenade_projectile")!=nullptr||!V_strcmp(name,"flashbang_projectile")||!V_strcmp(name,"molotov_projectile")||!V_strcmp(name,"decoy_projectile");
        if(!smoke&&(!projectile&&(!weapon||weapon->GetOwner())))continue;
        if(!smoke&&e->IsEffectActive(EF_NODRAW))continue;
        const char* model=weapon?weapon->GetWorldModel():STRING(e->GetModelName());
        if(!smoke&&(!model||!model[0]))continue;
        auto& w=out.world[out.worldCount++];w.id=e->entindex();w.kind=smoke?2:projectile?1:0;
        V_strncpy(w.classname,name,sizeof(w.classname));if(model)V_strncpy(w.model,model,sizeof(w.model));copy(w.origin,e->GetAbsOrigin());
        for(int i=0;i<3;++i)w.angles[i]=e->GetAbsAngles()[i];
        if(auto* animated=dynamic_cast<CBaseAnimating*>(e)){w.skin=animated->m_nSkin.Get();w.body=animated->m_nBody.Get();}
    }
    V_strncpy(out.model,STRING(p->GetModelName()),sizeof(out.model));
    out.modelBody=p->m_nBody.Get();
    bones(p,out.modelBones,out.modelBoneCount,false);
    animation(p,out.modelAnimation,sizeof(out.modelAnimation),out.modelCycle,out.modelSkin);
    if(auto* weapon=p->GetActiveCSWeapon()){
        V_strncpy(out.weapon,weapon->GetClassname(),sizeof(out.weapon));
        V_strncpy(out.viewModel,weapon->GetViewModel(),sizeof(out.viewModel));
        if(auto* vm=p->GetViewModel()){out.viewBody=vm->m_nBody.Get();const char* model=STRING(vm->GetModelName());if(model&&model[0])V_strncpy(out.viewModel,model,sizeof(out.viewModel));}
        out.clip=weapon->Clip1();out.reserve=p->GetAmmoCount(weapon->GetPrimaryAmmoType());
        bones(p->GetViewModel(),out.viewBones,out.viewBoneCount,true);
        animation(p->GetViewModel(),out.viewAnimation,sizeof(out.viewAnimation),out.viewCycle,out.viewSkin);
    }
    return true;
}
}
