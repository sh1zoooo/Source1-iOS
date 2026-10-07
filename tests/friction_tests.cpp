#include "Runtime.hpp"
#include "tier1/interface.h"
#include "tier3/tier3.h"
#include "vphysics_interface.h"
#include "vphysics/friction.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
int main(){
    const auto root=std::filesystem::temp_directory_path()/"source1ios-friction-test";
    try{
        source1ios::Runtime runtime;if(!runtime.start(root))throw std::runtime_error("Host failed");
        auto* physics=static_cast<IPhysics*>(Sys_GetFactoryThis()(VPHYSICS_INTERFACE_VERSION,nullptr));
        auto* env=physics->CreateEnvironment();env->SetGravity(Vector(0,0,-600));
        objectparams_t params{nullptr,1,1,0,0,.05f,"friction regression",nullptr,0,1,true};
        auto* shape=g_pPhysicsCollision->BBoxToCollide(Vector(-64,-64,-8),Vector(64,64,0));
        auto* floor=env->CreatePolyObjectStatic(shape,0,Vector(0,0,0),QAngle(0,0,0),&params);
        auto* body=env->CreateSphereObject(4,0,Vector(0,0,16),QAngle(0,0,0),&params,false);body->Wake();
        bool checked=false;
        for(int step=0;step<100;++step){
            env->Simulate(.015f);
            auto* snapshot=body->CreateFrictionSnapshot();
            while(snapshot->IsValid()){
                Vector normal;snapshot->GetSurfaceNormal(normal);
                if(!normal.IsValid()||std::abs(normal.Length()-1)>.01f||std::abs(normal.z)<.9f)throw std::runtime_error("Invalid retained contact normal after simulation");
                checked=true;snapshot->NextFrictionData();
            }
            body->DestroyFrictionSnapshot(snapshot);
        }
        env->DestroyObject(body);env->DestroyObject(floor);physics->DestroyEnvironment(env);g_pPhysicsCollision->DestroyCollide(shape);
        if(!checked)throw std::runtime_error("Floor contact was never observed");
        runtime.stop();std::filesystem::remove_all(root);std::cout<<"Persistent post-simulation friction normals: PASS\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
