#include "SourceStudio.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>

// A deterministic bounds probe, not exhaustive fuzzing or leak validation.
// LeakSanitizer cannot enumerate tasks in the restricted local workspace.
extern "C" int __lsan_is_turned_off(){return 1;}
extern "C" const char* __asan_default_options(){return "detect_leaks=0";}
int main(){
    MathLib_Init(2.2f,2.2f,0,2);
    unsigned accepted=0,rejected=0;std::uint32_t state=0x510519;
    auto random=[&](){state^=state<<13;state^=state>>17;state^=state<<5;return state;};
    for(unsigned multiple=0;multiple<2;++multiple)for(unsigned legacy=0;legacy<2;++legacy)for(unsigned external=0;external<2;++external){auto fixture=source1ios::makeStudioFixture(external,multiple);if(legacy){const int version=48;std::memcpy(fixture.mdl.data()+4,&version,4);}
        source1ios::StudioMesh golden;std::string goldenError;
        if(!source1ios::parseStudioModel(fixture.mdl,fixture.vvd,fixture.vtx,golden,goldenError,fixture.ani))throw std::runtime_error("Golden fixture rejected: "+goldenError);
        for(unsigned i=0;i<3000;++i){auto changed=fixture;const unsigned slot=i%(external?4:3);
            auto& file=slot==0?changed.mdl:slot==1?changed.vvd:slot==2?changed.vtx:changed.ani;
            if(i%5==0)file.resize(random()%file.size());
            else for(unsigned j=0,n=1+random()%4;j<n;++j)file[random()%file.size()]^=1u<<(random()%8);
            source1ios::StudioMesh mesh;std::string error;
            if(!source1ios::parseStudioModel(changed.mdl,changed.vvd,changed.vtx,mesh,error,changed.ani)){++rejected;continue;}
            ++accepted;source1ios::StudioPose pose;std::vector<source1ios::StudioVertex> output;
            if(!mesh.animations.empty()&&!source1ios::sampleStudioAnimation(mesh,0,.375,pose))throw std::runtime_error("sample rejected accepted clip");
            if(!source1ios::skinStudioModel(mesh,pose.rotations,output,pose.positions))throw std::runtime_error("skin rejected accepted model");
            for(const auto& vertex:output)if(!vertex.position.IsValid()||!vertex.normal.IsValid()||vertex.material>=std::max(size_t(1),mesh.materials.size()))throw std::runtime_error("nonfinite output or invalid material slot");
        }
    }
    std::cout<<"Studio mutation checks passed: 24000 cases (MDL48/49, single/multiple materials), "<<accepted<<" accepted, "<<rejected<<" rejected\n";
}
