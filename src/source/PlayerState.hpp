#pragma once
namespace source1ios {
struct PlayerState {
    bool active=false, alive=false, grounded=false, crouched=false;
    int health=0, armor=0, money=0, team=0, clip=-1, reserve=0, tick=0;
    float origin[3]{}, eye[3]{}, angles[3]{}, velocity[3]{};
    char weapon[64]{}, model[128]{}, viewModel[128]{};
    char modelAnimation[64]{}, viewAnimation[64]{};
    float modelCycle=0, viewCycle=0;
    int modelSkin=0, viewSkin=0;
    int modelBoneCount=0,viewBoneCount=0;
    float modelBones[128][12]{},viewBones[128][12]{};
};
// Implemented in the original GameDLL compile context; no SDK class layout
// crosses into UIKit or the host's independent compilation context.
bool gamePlayerSpawn(void* edict, int team);
bool gamePlayerRead(void* edict, PlayerState& state);
}
