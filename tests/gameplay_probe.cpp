#include "Runtime.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <filesystem>
using namespace source1ios;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void advance(Runtime& runtime,double seconds,double fps=60){for(int i=0;i<int(std::round(seconds*fps));++i)runtime.frame(1/fps);}
int main(int argc,char** argv){
    try{
        require(argc==3,"usage: gameplay_probe DOCUMENTS MAP");
        Runtime runtime;require(runtime.start(std::filesystem::absolute(argv[1])),"Host initialization failed");
        for(int cycle=0;cycle<2;++cycle){
            require(runtime.startGame(argv[2]),runtime.gameError().c_str());
            advance(runtime,3.5);
            auto start=runtime.playerState();
            require(start.active&&start.alive&&start.health==100&&start.grounded,"Original alive grounded player missing");
            std::cerr<<"Bone pose counts: player="<<start.modelBoneCount<<" view="<<start.viewBoneCount<<"\n";
            require(start.modelBoneCount>0&&start.viewBoneCount>0,"Original studio bone poses missing");
            require(start.model[0]&&start.viewModel[0]&&start.weapon[0],"Original inventory/model state missing");
            std::cout<<"Spawn: hp="<<start.health<<" weapon="<<start.weapon<<" clip="<<start.clip<<" reserve="<<start.reserve<<" model="<<start.model<<" view="<<start.viewModel<<'\n';
            runtime.cameraLook(-start.angles[1],0);runtime.cameraMove(1,0,0);advance(runtime,1.5,cycle?120:60);runtime.cameraMove(0,0,0);
            const auto walk=runtime.playerState();
            std::cerr<<"Movement diagnostic: tick="<<start.tick<<"->"<<walk.tick<<" angles="<<start.angles[0]<<","<<start.angles[1]<<" origin="<<start.origin[0]<<","<<start.origin[1]<<","<<start.origin[2]<<" -> "<<walk.origin[0]<<","<<walk.origin[1]<<","<<walk.origin[2]<<" velocity="<<walk.velocity[0]<<","<<walk.velocity[1]<<" alive="<<walk.alive<<"\n";
            require(std::hypot(walk.origin[0]-start.origin[0],walk.origin[1]-start.origin[1])>8,"Real usercmd walking failed");
            // Practice keeps the first spawn instead of relocating players in a match restart.
            // Walk to the actual boundary before asserting that the hull stops.
            runtime.cameraMove(1,0,0);advance(runtime,8);runtime.cameraMove(0,0,0);
            advance(runtime,.5);const auto wall=runtime.playerState();
            runtime.cameraMove(1,0,0);advance(runtime,1);runtime.cameraMove(0,0,0);
            require(std::hypot(runtime.playerState().origin[0]-wall.origin[0],runtime.playerState().origin[1]-wall.origin[1])<2,"Player passed the awp_lego_2 wall");
            const auto stand=runtime.playerState();runtime.playerButton(PlayerDuck,true);advance(runtime,.6);
            require(runtime.playerState().crouched&&runtime.playerState().eye[2]<stand.eye[2]-10,"Original crouch/eye height failed");
            bool bent=false;const auto duck=runtime.playerState();
            for(int bone=0;bone<std::min(stand.modelBoneCount,duck.modelBoneCount);++bone)
                for(int row=0;row<3;++row)for(int col=0;col<3;++col)bent|=std::abs(stand.modelBones[bone][row*4+col]-duck.modelBones[bone][row*4+col])>.01f;
            require(bent,"Original crouch animation did not change the player bone pose");
            runtime.playerButton(PlayerDuck,false);advance(runtime,.6);
            const float ground=runtime.playerState().origin[2];float peak=ground;
            runtime.playerButton(PlayerJump,true);
            for(int i=0;i<12;++i){runtime.frame(1./60);peak=std::max(peak,runtime.playerState().origin[2]);}
            runtime.playerButton(PlayerJump,false);
            for(int i=0;i<90;++i){runtime.frame(1./60);peak=std::max(peak,runtime.playerState().origin[2]);}
            require(peak>ground+20&&runtime.playerState().grounded,"Original jump/landing failed");
            auto clip=runtime.playerState().clip;
            runtime.playerButton(PlayerAttack,true);float maxPunch=0;
            for(int frame=0;frame<120;++frame){runtime.frame(1./60);maxPunch=std::max(maxPunch,std::abs(runtime.playerState().punch[0]));}
            runtime.playerButton(PlayerAttack,false);
            require(maxPunch>.1f,"Original recoil punch was not exported to the view adapter");
            require(runtime.playerState().clip<clip,"Original weapon did not consume ammunition");
            const auto fired=runtime.playerState();
            runtime.playerButton(PlayerReload,true);advance(runtime,5);runtime.playerButton(PlayerReload,false);
            require(runtime.playerState().clip>fired.clip&&runtime.playerState().reserve<fired.reserve,"Original reload did not transfer reserve ammunition to the magazine");
            std::cout<<"Ammo transition: "<<clip<<" -> "<<fired.clip<<" -> "<<runtime.playerState().clip<<"; reserve "<<fired.reserve<<" -> "<<runtime.playerState().reserve<<'\n';
            runtime.thirdPerson(true);advance(runtime,.1);auto vertices=runtime.vertices(1.5f);require(!vertices.empty(),"Third person geometry empty");
            for(const auto& vertex:vertices)for(float value:vertex.position)require(std::isfinite(value),"Nonfinite gameplay projection");
            auto visible=[](const std::vector<SourceVertex>& mesh){size_t count=0;for(const auto& v:mesh){const auto* p=v.position;
                if(v.material[0]<0&&p[3]>0&&std::abs(p[0])<p[3]&&std::abs(p[1])<p[3]&&p[2]>=0&&p[2]<p[3])++count;}return count;};
            require(visible(vertices)>100,"Third person player model outside the camera frustum");
            runtime.thirdPerson(false);advance(runtime,.1);require(visible(runtime.vertices(1.5f))>100,"First person weapon outside the camera frustum");
            const auto paused=runtime.playerState();
            runtime.cameraMove(-1,0,0);runtime.playerButton(PlayerDuck,true);runtime.playerButton(PlayerAttack,true);runtime.setActive(false);
            const int tick=runtime.playerState().tick;advance(runtime,1);require(runtime.playerState().tick==tick,"Paused gameplay advanced");
            runtime.setActive(true);advance(runtime,1);require(!runtime.playerState().crouched&&runtime.playerState().clip==paused.clip
                &&std::hypot(runtime.playerState().origin[0]-paused.origin[0],runtime.playerState().origin[1]-paused.origin[1])<2,"Movement/fire/duck input remained held after pause");
            std::cout<<"Gameplay cycle "<<cycle<<": PASS; walking/wall/duck/jump/fire/reload/1P/3P/pause; jump="<<peak-ground<<'\n';
            runtime.stopGame();require(!runtime.playerState().active,"Practice did not stop");
        }
        runtime.stop();return 0;
    }catch(const std::exception& error){std::cerr<<"Gameplay FAIL: "<<error.what()<<'\n';return 1;}
}
