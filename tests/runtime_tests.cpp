#include "Runtime.hpp"
#include "BspLzmaFixture.hpp"
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <vector>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("MixedCase-IOS-Container-" + std::string(90, 'A')) /
        ("source1ios-test-" + std::to_string(
            std::filesystem::file_time_type::clock::now().time_since_epoch().count()));
    try {
        source1ios::Runtime host;
        host.frame(0.016);
        check(host.frames() == 0, "Stopped host accepted a frame");
        check(host.start(directory), "Could not start host");
        check(host.sourceReady(), "Real Source modules did not start");
        const auto bspTexture=host.texture();const auto bspTextureRevision=host.textureRevision();
        check(bspTexture.width==128&&bspTexture.height==64&&bspTexture.pixels.size()==128*64*4,"BSP material atlas missing");
        bool material0=false,material1=false;for(const auto& vertex:host.vertices(1)){material0|=vertex.material[0]==0;material1|=vertex.material[0]==1;}
        check(material0&&material1,"BSP surface material slots were not preserved in render vertices");
        static_assert(sizeof(source1ios::SourceVertex)==64,"Metal/C++ vertex layout diverged");
        const auto bakedTexture=host.lightmapTexture();
        check(bakedTexture.width==1024&&bakedTexture.height==1024&&bakedTexture.pixels.size()==1024*1024*4,"BSP lightmap atlas missing");
        const auto roomVertices=host.vertices(1);
        for(size_t i=0;i<252;++i)check(roomVertices[i].lightmap[2]==1&&roomVertices[i].lightmap[0]>0&&roomVertices[i].lightmap[0]<1&&roomVertices[i].lightmap[1]>0&&roomVertices[i].lightmap[1]<1,"BSP lit vertex lacks bounded atlas coordinates");
        for(size_t i=252;i<288;++i)check(roomVertices[i].lightmap[2]==0,"Studio model incorrectly samples world lightmap");
        check(bakedTexture.pixels[0]==bakedTexture.pixels[4]&&bakedTexture.pixels[0]<bakedTexture.pixels[33*4],"Lightmap border or gradient sampling incorrect");
        check(host.executeSource("source_bsp_selftest"), "BSP geometry/collision contracts failed");
        check(host.executeSource("source_bsp_materials"),"Seventeen-material BSP demo failed");
        const auto gridTexture=host.texture();check(gridTexture.width==1024&&gridTexture.height==128,"Material grid dimensions incorrect");
        bool secondRow=false;for(const auto& vertex:host.vertices(1))secondRow|=vertex.material[0]==16&&vertex.material[1]==17;check(secondRow,"Second-row material slot not staged in GPU vertices");
        check(std::memcmp(gridTexture.pixels.data()+64*1024*4,bspTexture.pixels.data()+64*4,64*4)==0,"Second-row material pixels incorrect");
        check(host.executeSource("source_bsp_reset"),"Two-slot map restore after material grid failed");
        check(host.executeSource("source_bsp_terrain"),"Built-in Source terrain demo failed");
        check(host.executeSource("source_bsp_reset"),"Original room restore after terrain failed");
        const auto beforeProps=host.vertices(1);check(host.executeSource("source_bsp_props"),"Static prop BSP load failed");const auto withProps=host.vertices(1);
        check(withProps.size()==beforeProps.size()+72&&host.texture().width==192,"Static prop instances or atlas missing");
        check(host.executeSource("source_props_selftest"),"Live static prop collision checks failed");
        check(host.executeSource("source_physics_reset")&&host.executeSource("source_props_selftest"),"Physics reset lost static prop collisions");
        for(size_t i=252;i<324;++i)check(withProps[i].material[0]==2&&withProps[i].material[1]==3&&withProps[i].lightmap[2]==0,"Static prop texture slot or lighting incorrect");
        const auto portraitProps=host.vertices(.46f);for(size_t first:{size_t(252),size_t(288)}){float center[4]{};for(size_t i=first;i<first+36;++i)for(int axis=0;axis<4;++axis)center[axis]+=portraitProps[i].position[axis]/36;check(center[3]>1&&std::abs(center[0])<center[3]&&std::abs(center[1])<center[3],"Static prop centroid is outside portrait viewport");}
        check(host.executeSource("source_bsp_reset")&&host.vertices(1).size()==beforeProps.size()&&host.texture().width==128,"Static props survived map reset");
        check(host.executeSource("source_model_reset"),"Built-in Source studio model reload failed");
        check(host.executeSource("source_model_load models/__source1ios_legacy_probe.mdl")&&host.executeSource("source_anim_play 0"),"MDL48 embedded model load/clip failed");
        check(host.executeSource("source_model_load models/__source1ios_external48_probe.mdl")&&host.executeSource("source_anim_play 0"),"MDL48 external ANI model load/clip failed");
        check(host.executeSource("source_model_load models/__source1ios_external_probe.mdl"),"External ANI model load failed");
        check(host.executeSource("source_anim_play 0"),"External ANI clip unavailable");const auto aniBefore=host.vertices(1);host.frame(.03);const auto aniAfter=host.vertices(1);
        bool aniMoved=false;for(size_t i=252;i<288;++i)for(int axis=0;axis<4;++axis)aniMoved|=aniBefore[i].position[axis]!=aniAfter[i].position[axis];check(aniMoved,"External ANI pose did not animate geometry");
        const auto aniFile=directory/"Source1IOS/game/models/__source1ios_external_probe.ani";
        std::ifstream aniInput(aniFile,std::ios::binary);std::vector<char> aniBytes((std::istreambuf_iterator<char>(aniInput)),{});aniInput.close();
        {std::ofstream brokenAni(aniFile,std::ios::binary);brokenAni.write(aniBytes.data(),aniBytes.size()-1);}
        const auto aniRevision=host.modelTextureRevision();check(!host.executeSource("source_model_load models/__source1ios_external_probe.mdl")&&host.modelTextureRevision()==aniRevision,"Malformed ANI replaced current model");
        std::filesystem::remove(aniFile);check(host.executeSource("source_model_load models/__source1ios_external_probe.mdl")&&!host.executeSource("source_anim_play 0"),"Missing ANI did not retain static geometry");
        {std::ofstream restoredAni(aniFile,std::ios::binary);restoredAni.write(aniBytes.data(),aniBytes.size());}
        check(host.executeSource("source_model_reset"),"Embedded model restore after ANI test failed");
        check(host.executeSource("source_anim_play 0"),"Embedded studio clip selection failed");
        check(!host.executeSource("source_anim_play -1")&&!host.executeSource("source_anim_play 999")&&!host.executeSource("source_anim_play 0junk"),"Invalid animation index accepted");
        check(!host.executeSource("source_model_load models/missing.mdl"),"Missing studio companions accepted");
        check(!host.executeSource("source_bsp_load \"\""), "Empty BSP name accepted");
        check(!host.executeSource("source_bsp_load maps/../../outside.bsp"), "BSP path traversal accepted");
        check(!host.executeSource("source_bsp_load maps/missing.bsp"), "Missing map accepted");
        check(host.texture().pixels==bspTexture.pixels,"Rejected map replaced BSP texture");
        check(host.executeSource("source_host_selftest"), "Original Host_Init / idle frame contracts failed");
        check(host.executeSource("source_engine_selftest"), "Engine subsystem contracts failed");
        check(host.executeSource("source_assets_selftest"), "Assets/physics contracts failed");
        check(!host.executeSource("map test"), "Game command exposed without a game DLL");
        check(host.executeSource("source_app_selftest"), "Appframework contracts failed");
        check(host.executeSource("source_fs_selftest"), "Original filesystem contracts failed");
        check(host.executeSource("source_selftest"), "Source contracts failed");
        check(host.executeSource("ios_rotation_speed 0"), "Source ConVar command failed");
        check(host.executeSource("source_physics_reset"), "Live physics reset failed");
        check(host.executeSource("source_physics_impulse"), "Live physics impulse failed");
        const auto physicsBefore=host.vertices(1);
        host.setActive(false);host.frame(.02);
        const auto physicsPaused=host.vertices(1);
        check(physicsBefore.back().position[0]==physicsPaused.back().position[0], "Paused physics advanced");
        host.setActive(true);
        const auto cameraBefore=host.vertices(1);
        host.cameraLook(10,0);
        const auto cameraAfter=host.vertices(1);
        check(!cameraBefore.empty() && cameraBefore[0].position[0]!=cameraAfter[0].position[0], "Camera look did not alter the view");
        check(host.executeSource("source_camera_reset"), "Camera reset failed");
        for(int i=0;i<2000;++i)host.cameraMove(1,0,.1f);
        const auto atWall=host.vertices(1);
        for(int i=0;i<20;++i)host.cameraMove(1,0,.1f);
        check(std::abs(host.vertices(1)[0].position[0]-atWall[0].position[0])<.001f, "Camera crossed a solid BSP wall");
        check(host.executeSource("source_camera_reset"), "Camera reset after movement failed");
        const auto maps=directory/"Source1IOS/game/maps";
        std::filesystem::create_directories(maps);
        std::filesystem::copy_file(directory/"Source1IOS/selftest/__source1ios_props.bsp",maps/"props.bsp");
        check(host.executeSource("source_bsp_load maps/props.bsp"),"Imported static prop BSP failed");const auto propRevision=host.textureRevision();const auto propGeometry=host.vertices(1);
        std::ifstream propsInput(maps/"props.bsp",std::ios::binary);std::vector<char> propBytes((std::istreambuf_iterator<char>(propsInput)),{});propsInput.close();
        int gameOffset=0,payloadOffset=0;std::memcpy(&gameOffset,propBytes.data()+8+35*16,4);std::memcpy(&payloadOffset,propBytes.data()+gameOffset+12,4);
        auto rejectProps=[&](const std::vector<char>& data){std::ofstream broken(maps/"badprops.bsp",std::ios::binary);broken.write(data.data(),data.size());broken.close();check(!host.executeSource("source_bsp_load maps/badprops.bsp")&&host.textureRevision()==propRevision&&host.vertices(1).size()==propGeometry.size(),"Malformed prop BSP replaced live scene");};
        auto invalidProps=propBytes;std::uint16_t badModel=1;std::memcpy(invalidProps.data()+payloadOffset+140+24,&badModel,2);rejectProps(invalidProps);
        invalidProps=propBytes;int badOffset=gameOffset;std::memcpy(invalidProps.data()+gameOffset+12,&badOffset,4);rejectProps(invalidProps);
        invalidProps=propBytes;std::uint16_t badVersion=99;std::memcpy(invalidProps.data()+gameOffset+10,&badVersion,2);rejectProps(invalidProps);
        invalidProps=propBytes;invalidProps[payloadOffset+140+30]=1;rejectProps(invalidProps);
        check(host.executeSource("source_camera_reset"),"Static prop camera reset failed");
        for(unsigned i=0;i<20;++i)host.cameraMove(1,0,.1f);const auto solidDepth=host.vertices(1)[0].position[3];
        const auto phyPath=directory/"Source1IOS/game/models/__source1ios_static_probe.phy";const auto hiddenPhy=directory/"Source1IOS/game/models/__source1ios_static_probe.phy.hidden";
        std::filesystem::rename(phyPath,hiddenPhy);
        for(unsigned solid:{0,6}){auto visualOnly=propBytes;visualOnly[payloadOffset+140+30]=solid;visualOnly[payloadOffset+196+30]=solid;
            std::ofstream out(maps/"visualprops.bsp",std::ios::binary);out.write(visualOnly.data(),visualOnly.size());out.close();
            check(host.executeSource("source_bsp_load maps/visualprops.bsp"),"Non-BBOX props lost their visual geometry");
            check(host.vertices(1).size()==propGeometry.size(),"Non-BBOX prop geometry changed");
            for(unsigned i=0;i<20;++i)host.cameraMove(1,0,.1f);
            check(solidDepth-host.vertices(1)[0].position[3]>30,"Non-BBOX props incorrectly gained BBOX collisions");}
        std::filesystem::rename(hiddenPhy,phyPath);
        check(host.executeSource("source_bsp_phy")&&host.executeSource("source_phy_selftest"),"Exact PHY static prop scene failed");
        check(host.executeSource("source_physics_reset")&&host.executeSource("source_phy_selftest"),"Exact PHY collisions lost after reset");
        std::ifstream phyIn(phyPath,std::ios::binary);std::vector<char> originalPhy((std::istreambuf_iterator<char>(phyIn)),{});phyIn.close();
        const auto phyRevision=host.textureRevision();auto brokenPhy=originalPhy;brokenPhy[12]^=1;
        {std::ofstream out(phyPath,std::ios::binary);out.write(brokenPhy.data(),brokenPhy.size());}
        check(!host.executeSource("source_bsp_phy")&&host.textureRevision()==phyRevision,"Corrupt PHY checksum replaced scene");
        {std::ofstream out(phyPath,std::ios::binary);out.write(originalPhy.data(),originalPhy.size()-8);}
        check(!host.executeSource("source_bsp_phy")&&host.textureRevision()==phyRevision,"Truncated PHY replaced scene");
        {std::ofstream out(phyPath,std::ios::binary);out.write(originalPhy.data(),originalPhy.size());}
        const auto ldrLight=host.lightmapTexture().pixels;
        check(host.executeSource("source_bsp_hdr")&&host.executeSource("source_hdr_selftest"),"HDR-only map did not load");
        check(host.lightmapTexture().pixels!=ldrLight,"HDR mapping did not change sampled atlas");
        check(host.executeSource("source_physics_reset")&&host.executeSource("source_phy_selftest"),"HDR load lost exact PHY collisions");
        const auto hdrPath=directory/"Source1IOS/selftest/__source1ios_hdr.bsp";
        std::ifstream hdrIn(hdrPath,std::ios::binary);std::vector<char> originalHdr((std::istreambuf_iterator<char>(hdrIn)),{});hdrIn.close();
        const auto hdrRevision=host.textureRevision();const auto hdrLight=host.lightmapTexture().pixels;auto corruptHdr=originalHdr;
        // BSP header: ident/version, then 64 records of four int32 fields.
        // Lump 53 is HDR lighting; an odd length must fail before legacy loading.
        std::int32_t length=0;std::memcpy(&length,corruptHdr.data()+8+53*16+4,4);--length;std::memcpy(corruptHdr.data()+8+53*16+4,&length,4);
        {std::ofstream out(hdrPath,std::ios::binary);out.write(corruptHdr.data(),corruptHdr.size());}
        check(!host.executeSource("source_bsp_hdr")&&host.textureRevision()==hdrRevision&&host.lightmapTexture().pixels==hdrLight,"Invalid HDR replaced live scene");
        corruptHdr=originalHdr;std::int32_t hdrFaceOffset=0;std::memcpy(&hdrFaceOffset,corruptHdr.data()+8+58*16,4);
        const std::int32_t invalidLightOffset=std::numeric_limits<std::int32_t>::max();
        std::memcpy(corruptHdr.data()+hdrFaceOffset+20,&invalidLightOffset,4);
        {std::ofstream out(hdrPath,std::ios::binary);out.write(corruptHdr.data(),corruptHdr.size());}
        check(!host.executeSource("source_bsp_hdr")&&host.textureRevision()==hdrRevision&&host.lightmapTexture().pixels==hdrLight,"Invalid HDR face light offset replaced scene");
        {std::ofstream out(hdrPath,std::ios::binary);out.write(originalHdr.data(),originalHdr.size());}
        check(host.executeSource("source_bsp_reset"),"Reset after invalid prop imports failed");
        std::filesystem::copy_file(directory/"Source1IOS/selftest/__source1ios_geometry.bsp",maps/"imported.bsp");
        const auto content=directory/"Source1IOS/content";const auto cm=content/"cm";std::filesystem::create_directories(cm/"maps");
        std::filesystem::copy_file(maps/"imported.bsp",cm/"maps/cache_probe.bsp");
        std::filesystem::create_directories(cm/"models");std::filesystem::create_directories(cm/"materials/models/source1ios");
        const auto gameModels=directory/"Source1IOS/game/models";
        std::ifstream modelInput(gameModels/"__source1ios_static_probe.mdl",std::ios::binary);std::vector<char> cacheMdl((std::istreambuf_iterator<char>(modelInput)),{});
        const std::string oldMaterial="__source1ios_model",newMaterial="__cache_texture";const auto materialName=std::search(cacheMdl.begin(),cacheMdl.end(),oldMaterial.begin(),oldMaterial.end());check(materialName!=cacheMdl.end(),"Fixture model material name missing");
        std::fill(materialName,materialName+oldMaterial.size(),0);std::copy(newMaterial.begin(),newMaterial.end(),materialName);
        {std::ofstream cachedModel(cm/"models/cache_probe.mdl",std::ios::binary);cachedModel.write(cacheMdl.data(),cacheMdl.size());}
        std::filesystem::copy_file(gameModels/"__source1ios_static_probe.vvd",cm/"models/cache_probe.vvd");std::filesystem::copy_file(gameModels/"__source1ios_static_probe.dx90.vtx",cm/"models/cache_probe.dx90.vtx");
        std::filesystem::copy_file(directory/"Source1IOS/game/materials/debug/debugblue.vtf",cm/"materials/models/source1ios/__cache_texture.vtf");
        {std::ofstream cacheVmt(cm/"materials/models/source1ios/__cache_texture.vmt");cacheVmt<<"VertexLitGeneric { \"$basetexture\" \"models/source1ios/__cache_texture\" }";}
        check(!host.executeSource("source_bsp_load maps/cache_probe.bsp"),"Unmounted content leaked into GAME search paths");
        check(host.executeSource("source_content_mount cm")&&host.executeSource("source_content_mount cm"),"Loose cache directory mount was not idempotent");
        check(host.executeSource("source_bsp_load maps/cache_probe.bsp"),"Mounted cache BSP could not be read through Source filesystem");
        check(host.executeSource("source_model_load models/cache_probe.mdl")&&host.modelTexture().width==64&&host.modelTexture().pixels[0]==90&&host.modelTexture().pixels[1]==150&&host.modelTexture().pixels[2]==210,"Mounted cache MDL companions or VMT/VTF material did not resolve");
        check(!host.executeSource("source_content_mount ../game")&&!host.executeSource("source_content_mount /tmp")&&!host.executeSource("source_content_mount missing")&&!host.executeSource("source_content_mount cm/archive.vpk"),"Invalid content path accepted");
        check(host.executeSource("source_content_unmount cm")&&!host.executeSource("source_content_unmount cm"),"Content unmount did not remove its search path");
        check(!host.executeSource("source_bsp_load maps/cache_probe.bsp"),"Unmounted content remained visible");
        check(!host.executeSource("source_model_load models/cache_probe.mdl"),"Unmounted model remained visible in search paths");
        check(host.executeSource("source_model_reset"),"Built-in model restore after content import failed");
        std::filesystem::create_directory_symlink(maps,content/"outside_alias");check(!host.executeSource("source_content_mount outside_alias"),"Symlink content root accepted");
        std::filesystem::create_directory_symlink(maps,cm/"nested_alias");check(!host.executeSource("source_content_mount cm"),"Symlink inside content accepted");std::filesystem::remove(cm/"nested_alias");
        {std::ofstream zip(cm/"zip0.zip");zip<<"malformed auto-pack";}check(!host.executeSource("source_content_mount cm"),"Implicit legacy zip archive mount accepted");std::filesystem::remove(cm/"zip0.zip");
        check(host.executeSource("source_content_mount cm"),"Valid content mount did not recover after rejected import");
        check(host.executeSource("source_bsp_load maps/imported.bsp"), "Valid user BSP preview failed");
        check(host.texture().pixels==bspTexture.pixels&&host.textureRevision()>bspTextureRevision,"Imported BSP material did not resolve or advance revision");
        // A CS spawn far from the fixture default must change the view, and
        // camera reset must restore the selected map spawn rather than (0,0,0).
        std::ifstream entityInput(maps/"imported.bsp",std::ios::binary);
        std::vector<unsigned char> spawnMap((std::istreambuf_iterator<char>(entityInput)),{});entityInput.close();
        auto entityFixture=[&](const std::string& text,const char* name){auto bytes=spawnMap;const int offset=bytes.size(),length=text.size()+1;bytes.insert(bytes.end(),text.begin(),text.end());bytes.push_back(0);
            std::memcpy(bytes.data()+8,&offset,4);std::memcpy(bytes.data()+12,&length,4);std::ofstream out(maps/name,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());};
        entityFixture("{ classname worldspawn } { classname info_player_counterterrorist origin \"-120 -80 16\" angles \"0 90 0\" }","team-spawn.bsp");
        const auto fixtureView=host.vertices(1);check(host.executeSource("source_bsp_load maps/team-spawn.bsp"),"CS team spawn map rejected");
        const auto spawnView=host.vertices(1);check(!spawnView.empty()&&fixtureView[0].position[0]!=spawnView[0].position[0],"Map spawn did not change camera projection");
        host.cameraLook(30,5);check(host.executeSource("source_camera_reset")&&host.vertices(1)[0].position[0]==spawnView[0].position[0],"Camera reset ignored map spawn");
        const auto spawnRevision=host.textureRevision();entityFixture("{ classname info_player_start origin \"nan 0 0\" }","bad-spawn.bsp");
        check(!host.executeSource("source_bsp_load maps/bad-spawn.bsp")&&host.textureRevision()==spawnRevision&&host.vertices(1)[0].position[0]==spawnView[0].position[0],"Malformed spawn changed active scene or camera");
        check(host.executeSource("source_bsp_load maps/imported.bsp"),"Map restore after spawn test failed");
        const auto imported=host.vertices(1);
        const auto models=directory/"Source1IOS/game/models";
        std::filesystem::copy_file(models/"__source1ios_static_probe.mdl",models/"bad.mdl");
        std::filesystem::copy_file(models/"__source1ios_static_probe.vvd",models/"bad.vvd");
        std::filesystem::copy_file(models/"__source1ios_static_probe.dx90.vtx",models/"bad.dx90.vtx");
        {std::fstream bad(models/"bad.vvd",std::ios::in|std::ios::out|std::ios::binary);bad.seekg(8);int checksum=0;bad.read(reinterpret_cast<char*>(&checksum),4);checksum^=1;bad.seekp(8);bad.write(reinterpret_cast<const char*>(&checksum),4);}
        check(!host.executeSource("source_model_load models/bad.mdl"),"Mismatched studio companions accepted");
        check(host.vertices(1).size()==imported.size(),"Rejected studio model replaced visible geometry");
        const auto textureBefore=host.modelTexture();const auto revisionBefore=host.modelTextureRevision();
        check(!host.executeSource("source_model_load models/bad.mdl")&&host.modelTexture().pixels==textureBefore.pixels&&host.modelTextureRevision()==revisionBefore,"Rejected model replaced texture or advanced GPU upload revision");
        const auto vmt=directory/"Source1IOS/game/materials/models/source1ios/__source1ios_model.vmt";
        {std::ofstream patch(vmt);patch<<"Patch { include \"materials/debug/debugempty.vmt\" replace { \"$basetexture\" \"models/source1ios/__source1ios_model\" } }";}
        check(host.executeSource("source_model_reset")&&host.modelTexture().pixels==textureBefore.pixels,"Model VMT Patch include/replace failed to resolve base texture");
        {std::ofstream patch(vmt);patch<<"Patch { include \"materials/models/source1ios/__source1ios_model.vmt\" }";}
        check(host.executeSource("source_model_reset")&&host.modelTexture().pixels==host.texture().pixels,"Cyclic VMT Patch did not use optional material fallback");
        {std::ofstream invalidVmt(vmt);invalidVmt<<"VertexLitGeneric { \"$basetexture\" \"../../outside\" }";}
        check(host.executeSource("source_model_reset"),"Missing/invalid optional material blocked model geometry");
        check(host.modelTexture().pixels==host.texture().pixels,"Invalid material did not use checker fallback");
        {std::ofstream restoredVmt(vmt);restoredVmt<<"VertexLitGeneric { \"$basetexture\" \"models/source1ios/__source1ios_model\" }";}
        check(host.executeSource("source_model_reset")&&host.modelTexture().pixels==textureBefore.pixels,"Studio texture restore failed");
        const auto vtf=vmt.parent_path()/"__source1ios_model.vtf";
        std::ifstream textureFile(vtf,std::ios::binary);
        std::vector<unsigned char> textureBytes((std::istreambuf_iterator<char>(textureFile)),{});
        textureFile.close();auto malformedTexture=textureBytes;
        check(malformedTexture.size()>88,"Fixture VTF resource table missing");
        const uint32_t resourceType=0x10,resourceOffset=malformedTexture.size(),hugeLength=0x7fffffff;
        std::memcpy(malformedTexture.data()+80,&resourceType,4);
        std::memcpy(malformedTexture.data()+84,&resourceOffset,4);
        const auto* lengthBytes=reinterpret_cast<const unsigned char*>(&hugeLength);
        malformedTexture.insert(malformedTexture.end(),lengthBytes,lengthBytes+4);
        {std::ofstream out(vtf,std::ios::binary);out.write(reinterpret_cast<const char*>(malformedTexture.data()),malformedTexture.size());}
        check(host.executeSource("source_model_reset")&&host.modelTexture().pixels==host.texture().pixels,"Oversized auxiliary VTF chunk was not rejected safely");
        {std::ofstream out(vtf,std::ios::binary);out.write(reinterpret_cast<const char*>(textureBytes.data()),textureBytes.size());}
        check(host.executeSource("source_model_reset")&&host.modelTexture().pixels==textureBefore.pixels,"Texture recovery after malformed resource failed");
        std::ifstream bspFile(maps/"imported.bsp",std::ios::binary);
        std::vector<unsigned char> bsp((std::istreambuf_iterator<char>(bspFile)),{});
        auto put32=[&](size_t offset,uint32_t value){std::memcpy(bsp.data()+offset,&value,4);};
        const auto compressed=source1ios::bspLzmaVertices();
        put32(8+3*16,bsp.size());put32(8+3*16+4,compressed.size());put32(8+3*16+12,672);
        put32(8+7*16+8,1); // Source's declared LUMP_FACES_VERSION.
        bsp.insert(bsp.end(),compressed.begin(),compressed.end());
        auto writeBsp=[&](const char* name){std::ofstream out(maps/name,std::ios::binary);out.write(reinterpret_cast<const char*>(bsp.data()),bsp.size());};
        writeBsp("compressed.bsp");
        check(host.executeSource("source_bsp_load maps/compressed.bsp"),"Compressed Source vertex lump rejected");
        const auto compressedView=host.vertices(1);
        check(compressedView.size()==imported.size(),"Compressed BSP changed mesh size");
        for(size_t i=0;i<imported.size();++i)for(int axis=0;axis<4;++axis)
            check(compressedView[i].position[axis]==imported[i].position[axis],"Compressed BSP changed geometry or physics pose");
        // A guaranteed invalid range decoder initial byte is the first payload byte.
        bsp[bsp.size()-compressed.size()+17]=255;writeBsp("damaged-compressed.bsp");
        check(!host.executeSource("source_bsp_load maps/damaged-compressed.bsp"),"Damaged LZMA stream accepted");
        check(host.vertices(1)[0].position[0]==compressedView[0].position[0],"Damaged compressed map replaced scene");
        // Raw LZMA1 stream: lc=3/lp=0/pb=2, 64 KiB dictionary, followed
        // by the Source 17-byte header. Decodes both material names and NULs.
        const unsigned char compressedNames[]={76,90,77,65,34,0,0,0,33,0,0,0,93,0,0,1,0,0,50,25,72,110,4,71,75,143,54,22,99,82,67,204,200,57,88,183,107,100,195,203,141,72,114,70,255,255,171,164,0,0};
        std::ifstream materialSource(maps/"imported.bsp",std::ios::binary);bsp.assign(std::istreambuf_iterator<char>(materialSource),{});
        const auto originalMaterials=bsp;
        uint32_t facesOffset=0;std::memcpy(&facesOffset,bsp.data()+8+7*16,4);
        // Source dface_t.lightofs is byte 20. Rejected lighting must retain
        // the last valid CPU scene and both GPU upload inputs.
        put32(facesOffset+20,0x7ffffffc);writeBsp("bad-lightmap-offset.bsp");
        const auto priorLightRevision=host.textureRevision();
        check(!host.executeSource("source_bsp_load maps/bad-lightmap-offset.bsp")&&host.textureRevision()==priorLightRevision&&host.lightmapTexture().pixels==bakedTexture.pixels,"Invalid lightmap offset replaced scene or lighting");
        bsp=originalMaterials;
        put32(8+43*16,bsp.size());put32(8+43*16+4,sizeof(compressedNames));put32(8+43*16+12,34);
        bsp.insert(bsp.end(),std::begin(compressedNames),std::end(compressedNames));writeBsp("compressed-materials.bsp");
        check(host.executeSource("source_bsp_load maps/compressed-materials.bsp"),"Compressed BSP material names rejected");
        check(host.texture().pixels==bspTexture.pixels,"Compressed BSP changed resolved materials");
        const auto materialRevision=host.textureRevision();
        bsp.back()^=255;writeBsp("bad-material-stream.bsp");
        check(!host.executeSource("source_bsp_load maps/bad-material-stream.bsp")&&host.textureRevision()==materialRevision&&host.texture().pixels==bspTexture.pixels,"Corrupt compressed materials replaced scene or texture");
        bsp=originalMaterials;uint32_t texinfoOffset=0;std::memcpy(&texinfoOffset,bsp.data()+8+6*16,4);
        const float nonfinite=std::numeric_limits<float>::quiet_NaN();std::memcpy(bsp.data()+texinfoOffset,&nonfinite,4);writeBsp("nonfinite-texinfo.bsp");
        check(!host.executeSource("source_bsp_load maps/nonfinite-texinfo.bsp")&&host.textureRevision()==materialRevision,"Nonfinite texture axes accepted or replaced scene");
        // Compression metadata in unused sections must not block geometry preview.
        std::ifstream originalFile(maps/"imported.bsp",std::ios::binary);
        bsp.assign(std::istreambuf_iterator<char>(originalFile),{});
        put32(8+40*16+12,12);writeBsp("unused-compression.bsp");
        check(host.executeSource("source_bsp_load maps/unused-compression.bsp"),"Unused compressed section blocked polygon preview");
        std::filesystem::copy_file(directory/"Source1IOS/selftest/__source1ios_displacement.bsp",maps/"terrain.bsp");
        check(host.executeSource("source_bsp_load maps/terrain.bsp"),"Source displacement map import failed");
        const auto terrainView=host.vertices(1);
        check(terrainView.size()==imported.size()+90,"Displacement was skipped or incorrectly triangulated");
        std::ifstream terrainFile(maps/"terrain.bsp",std::ios::binary);bsp.assign(std::istreambuf_iterator<char>(terrainFile),{});
        uint32_t dispOffset=0;std::memcpy(&dispOffset,bsp.data()+8+26*16,4);put32(dispOffset+20,31);
        writeBsp("bad-terrain.bsp");
        check(!host.executeSource("source_bsp_load maps/bad-terrain.bsp"),"Unsafe displacement power accepted");
        check(host.vertices(1).size()==terrainView.size(),"Rejected displacement replaced the scene");
        check(host.executeSource("source_bsp_load maps/imported.bsp"),"Map restore after terrain failed");
        std::ofstream(maps/"invalid.bsp") << "short";
        check(!host.executeSource("source_bsp_load maps/invalid.bsp"), "Truncated BSP accepted");
        check(host.vertices(1)[0].position[0]==imported[0].position[0], "Rejected BSP replaced the previous map");
        check(host.executeSource("source_bsp_reset"), "Built-in BSP restore failed");
        const auto before = host.vertices(1);
        host.frame(0.01);
        const auto after = host.vertices(1);
        check(before[0].position[0] == after[0].position[0], "Stationary camera changed without input");
        bool modelMoves=false;for(size_t i=252;i<288;++i)for(int axis=0;axis<4;++axis){check(std::isfinite(after[i].position[axis]),"Studio pose produced nonfinite draw vertex");modelMoves|=before[i].position[axis]!=after[i].position[axis];}
        check(modelMoves,"Weighted studio pose did not advance with host frame");
        check(before.back().position[0]!=after.back().position[0], "Visible physics pose did not advance");
        check(!host.executeSource("missing_source_command"), "Unknown command accepted");
        check(!host.start(directory), "Duplicate start accepted");
        check(host.executeSource("source_anim_pause"),"Animation pause command failed");const auto pausedModel=host.vertices(1);
        host.frame(0.016);
        const auto pausedFrame=host.vertices(1);for(size_t i=252;i<288;++i)for(int axis=0;axis<4;++axis)check(pausedFrame[i].position[axis]==pausedModel[i].position[axis],"Paused animation advanced");
        check(host.executeSource("source_anim_resume"),"Animation resume command failed");
        host.setActive(false);
        host.frame(1);
        check(host.frames() == 3, "Background host advanced simulation");
        host.setActive(true);
        host.frame(30);
        check(std::abs(host.elapsed() - 0.156) < 1e-9, "Background interval was not clamped");
        host.frame(std::numeric_limits<double>::quiet_NaN());
        host.frame(std::numeric_limits<double>::infinity());
        host.frame(-1);
        check(host.frames() == 4, "Invalid delta was accepted");
        const auto logPath = host.logPath();
        host.stop();
        host.stop();
        std::ifstream file(logPath);
        std::string contents((std::istreambuf_iterator<char>(file)), {});
        check(contents.find("Host paused") != std::string::npos, "Pause log missing");
        check(contents.find("Host stopped after 4 frames") != std::string::npos, "Shutdown log missing");
        check(contents.find("Source Host_Shutdown completed; host_initialized=0") != std::string::npos, "Original host did not shut down");
        check(contents.find("Source host GAME directory: " + (directory / "Source1IOS" / "game").string()) != std::string::npos, "Host path differs from the mounted game path");
        check(contents.find("Recursive shutdown") == std::string::npos, "Recursive shutdown guard persisted");
        check(host.start(directory), "Restart failed");
        check(host.executeSource("source_bsp_load maps/cache_probe.bsp"),"Known cm resource folder was not mounted after restart");
        check(host.frames() == 0 && host.elapsed() == 0, "Restart retained simulation state");
        check(host.executeSource("source_host_selftest"), "Original host failed after restart");
        host.stop();
        const auto badPath = directory / "regular-file";
        std::ofstream(badPath) << "not a directory";
        check(!host.start(badPath), "Invalid document directory accepted");
        check(!host.running(), "Failed startup left host running");
        std::filesystem::remove_all(directory);
        std::cout << "Runtime contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(directory);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
