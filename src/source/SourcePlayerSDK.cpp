#include "PlayerState.hpp"
#include "cbase.h"
#include "cs_player.h"
#include "weapon_csbase.h"
#include "cs_shareddefs.h"
#include "baseviewmodel_shared.h"
#include "tier1/strtools.h"

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
bool gamePlayerSpawn(void* entity,int team) {
    auto* p=player(entity);if(!p||!p->IsBot()||(team!=TEAM_TERRORIST&&team!=TEAM_CT))return false;
    p->ChangeTeam(team);
    p->HandleCommand_JoinClass(0);
    // Offline practice uses the original round-respawn path, including inventory,
    // player model, VPhysics hull and the active player state.
    p->RoundRespawn();
    return p->IsAlive()&&!p->IsObserver();
}
bool gamePlayerRead(void* entity,PlayerState& out) {
    auto* p=player(entity);out={};if(!p)return false;
    out.active=true;out.alive=p->IsAlive();out.grounded=(p->GetFlags()&FL_ONGROUND)!=0;
    out.crouched=(p->GetFlags()&FL_DUCKING)!=0;out.health=p->GetHealth();out.armor=p->ArmorValue();
    out.simulationTime=gpGlobals->curtime;out.money=p->m_iAccount;out.team=p->GetTeamNumber();out.tick=gpGlobals->tickcount;
    copy(out.origin,p->GetAbsOrigin());copy(out.eye,p->EyePosition());copy(out.velocity,p->GetAbsVelocity());
    const auto angles=p->EyeAngles();const auto punch=p->GetPunchAngle();
    for(int i=0;i<3;++i){out.angles[i]=angles[i];out.punch[i]=punch[i];}
    V_strncpy(out.model,STRING(p->GetModelName()),sizeof(out.model));
    bones(p,out.modelBones,out.modelBoneCount,false);
    animation(p,out.modelAnimation,sizeof(out.modelAnimation),out.modelCycle,out.modelSkin);
    if(auto* weapon=p->GetActiveCSWeapon()){
        V_strncpy(out.weapon,weapon->GetClassname(),sizeof(out.weapon));
        V_strncpy(out.viewModel,weapon->GetViewModel(),sizeof(out.viewModel));
        out.clip=weapon->Clip1();out.reserve=p->GetAmmoCount(weapon->GetPrimaryAmmoType());
        bones(p->GetViewModel(),out.viewBones,out.viewBoneCount,true);
        animation(p->GetViewModel(),out.viewAnimation,sizeof(out.viewAnimation),out.viewCycle,out.viewSkin);
    }
    return true;
}
}
