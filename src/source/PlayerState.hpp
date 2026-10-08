#pragma once
namespace source1ios {
struct WorldEntityState {int id=0,kind=0,skin=0,body=0;float origin[3]{},angles[3]{};char model[128]{},classname[64]{};};
struct PlayerState {
    bool active=false, alive=false, grounded=false, crouched=false;
    int health=0, armor=0, money=0, team=0, clip=-1, reserve=0, tick=0;
    float origin[3]{}, eye[3]{}, angles[3]{}, velocity[3]{}, punch[3]{};
    char weapon[64]{}, model[128]{}, viewModel[128]{};
    char modelAnimation[64]{}, viewAnimation[64]{};
    float modelCycle=0, viewCycle=0, simulationTime=0, fov=90;
    float flashAlpha=0;
    int worldCount=0;WorldEntityState world[128]{};
    int modelSkin=0, viewSkin=0, modelBody=0, viewBody=0;
    int modelBoneCount=0,viewBoneCount=0;
    float modelBones[128][12]{},viewBones[128][12]{};
};
// Implemented in the original GameDLL compile context; no SDK class layout
// crosses into UIKit or the host's independent compilation context.
int gamePlayerBuy(void* entity,const char* alias);
bool gameBuyInfo(const char* alias,int& price,int& team);
bool gamePlayerAction(void* entity,const char* action);
bool gamePlayerSpawn(void* edict, int team);
bool gamePlayerRead(void* edict, PlayerState& state);
void gamePlayerAdvanceView(void* edict,float seconds);
}
