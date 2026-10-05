#include "Runtime.hpp"
#include "BspLzmaFixture.hpp"
#include <cstdint>
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
        check(bspTexture.width==64&&bspTexture.height==64&&bspTexture.pixels.size()==64*64*4,"BSP material texture missing");
        check(host.executeSource("source_bsp_selftest"), "BSP geometry/collision contracts failed");
        check(host.executeSource("source_bsp_terrain"),"Built-in Source terrain demo failed");
        check(host.executeSource("source_bsp_reset"),"Original room restore after terrain failed");
        check(host.executeSource("source_model_reset"),"Built-in Source studio model reload failed");
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
        std::filesystem::copy_file(directory/"Source1IOS/selftest/__source1ios_geometry.bsp",maps/"imported.bsp");
        check(host.executeSource("source_bsp_load maps/imported.bsp"), "Valid user BSP preview failed");
        check(host.texture().pixels==bspTexture.pixels&&host.textureRevision()>bspTextureRevision,"Imported BSP material did not resolve or advance revision");
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
        check(host.frames() == 2, "Background host advanced simulation");
        host.setActive(true);
        host.frame(30);
        check(std::abs(host.elapsed() - 0.126) < 1e-9, "Background interval was not clamped");
        host.frame(std::numeric_limits<double>::quiet_NaN());
        host.frame(std::numeric_limits<double>::infinity());
        host.frame(-1);
        check(host.frames() == 3, "Invalid delta was accepted");
        const auto logPath = host.logPath();
        host.stop();
        host.stop();
        std::ifstream file(logPath);
        std::string contents((std::istreambuf_iterator<char>(file)), {});
        check(contents.find("Host paused") != std::string::npos, "Pause log missing");
        check(contents.find("Host stopped after 3 frames") != std::string::npos, "Shutdown log missing");
        check(contents.find("Source Host_Shutdown completed; host_initialized=0") != std::string::npos, "Original host did not shut down");
        check(contents.find("Source host GAME directory: " + (directory / "Source1IOS" / "game").string()) != std::string::npos, "Host path differs from the mounted game path");
        check(contents.find("Recursive shutdown") == std::string::npos, "Recursive shutdown guard persisted");
        check(host.start(directory), "Restart failed");
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
