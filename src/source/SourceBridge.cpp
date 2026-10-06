#include "SourceBridge.hpp"
#include "SourceEngine.hpp"
#include "SourceAssets.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include "tier0/icommandline.h"
#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"
#include "tier1/checksum_crc.h"
#include "tier1/bitbuf.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "tier1/tier1.h"
#include "mathlib/mathlib.h"
#include "icvar.h"
#include "vstdlib/cvar.h"

namespace {
source1ios::SourceBridge::Logger logger = nullptr;
void* logContext = nullptr;
bool bridgeInUse = false;
ICvar* console = nullptr;
ConVar rotationSpeed("ios_rotation_speed", "30", FCVAR_NONE,
    "Angular speed of the generated Source math test scene", true, 0, true, 180);
SpewRetval_t sourceSpew(SpewType_t type, const char* message) {
    if (logger) logger(logContext, message);
    return type == SPEW_ERROR ? SPEW_ABORT : SPEW_CONTINUE;
}
void statusCommand(const CCommand&) {
    Msg("Source modules active: tier0, tier1, mathlib, vstdlib, filesystem_stdio, vpklib, appframework, tier2, tier3, bitmap, engine (dedicated), materialsystem/shaderapiempty, VTF, datacache, studiorender, vphysics/IVP.\n");
    Msg("Original Host_Init and idle frames active; BSP polygon preview uses a Metal adapter. Built-in engine brush world loaded; original Source graphical shaders and game DLL remain pending.\n");
}
ConCommand status("source_status", statusCommand, "Report the actual port scope");
void logCheck(const char* name, bool passed) {
    Msg("Source self-test %s: %s\n", name, passed ? "PASS" : "FAIL");
}
}

namespace source1ios {
bool SourceBridge::start(Logger output, void* context, const std::filesystem::path& root) {
    if (ready_ || bridgeInUse) return false;
    bridgeInUse = true;
    ownsCore_ = true;
    logger = output;
    logContext = context;
    SpewOutputFunc(sourceSpew);
    DeclareCurrentThreadIsMainThread();
    Plat_SetCommandLine("source1-ios -nowatchdog");
    CommandLine()->CreateCmdLine("source1-ios -nowatchdog");
    if (!systems_.start()) { stop(); return false; }
    console = static_cast<ICvar*>(systems_.find(CVAR_INTERFACE_VERSION));
    if (!console) { stop(); return false; }
    // Source normally registers once per DLL load. Static iOS modules stay loaded
    // across host restarts, so restore our commands after a disconnect/reconnect.
    if (!console->FindVar("ios_rotation_speed")) console->RegisterConCommand(&rotationSpeed);
    if (!console->FindCommand("source_status")) console->RegisterConCommand(&status);
    rotationSpeed.SetValue(30.0f);
    elapsed_ = 0;
    ready_ = true;
    const auto* cpu = GetCPUInformation();
    Msg("Source upstream: ed8209cc35c61fbd8ddff8480962a01c981eef2f\n");
    Msg("Source CPU: %s; logical processors: %u; pointer width: %u\n",
        cpu->m_szProcessorID, cpu->m_nLogicalProcessors, unsigned(sizeof(void*) * 8));
    Msg("Source factory: %s initialized\n", CVAR_INTERFACE_VERSION);
    if (!selfTest()) { stop(); return false; }
    if (!files_.start(root, systems_.find("VFileSystem022"))) { stop(); return false; }
    if (!sourceEngineSelfTest()) { stop(); return false; }
    if (!sourceAssetsSelfTest()) { stop(); return false; }
    if (!host_.start(root)) { stop(); return false; }
    if (!map_.start(root)) { stop(); return false; }
    execute("source_status");
    Msg("Source core initialized: tier0/tier1/mathlib/vstdlib\n");
    return true;
}
void SourceBridge::stop() {
    if (!ownsCore_) return;
    map_.stop();
    host_.stop();
    files_.stop();
    systems_.stop();
    console = nullptr;
    if (ready_) Msg("Source core shutdown\n");
    ready_ = false;
    bridgeInUse = false;
    ownsCore_ = false;
    logger = nullptr;
    logContext = nullptr;
    SpewOutputFunc(nullptr);
}
bool SourceBridge::selfTest() {
    if (!ready_) return false;
    bool all = true;
    const char* sample = "123456789";
    CRC32_t crc;
    CRC32_Init(&crc);
    CRC32_ProcessBuffer(&crc, sample, 9);
    CRC32_Final(&crc);
    bool passed = crc == 0xcbf43926;
    logCheck("CRC32", passed); all &= passed;
    alignas(4) unsigned char bytes[32] = {};
    bf_write writer(bytes, sizeof(bytes));
    writer.WriteUBitLong(0x5a3, 12);
    writer.WriteSBitLong(-17, 9);
    writer.WriteBitFloat(1.25f);
    bf_read reader(bytes, sizeof(bytes));
    passed = reader.ReadUBitLong(12) == 0x5a3 && reader.ReadSBitLong(9) == -17
        && reader.ReadBitFloat() == 1.25f && !reader.IsOverflowed() && !writer.IsOverflowed();
    logCheck("bitbuf", passed); all &= passed;
    auto* kv = new KeyValues("SourcePort");
    passed = kv->LoadFromBuffer("source-port-test", "\"SourcePort\" { \"platform\" \"ios\" \"bits\" \"64\" }")
        && !std::strcmp(kv->GetString("platform"), "ios") && kv->GetInt("bits") == 64;
    kv->deleteThis();
    logCheck("KeyValues", passed); all &= passed;
    matrix3x4_t matrix;
    AngleMatrix(QAngle(0, 90, 0), matrix);
    Vector transformed;
    VectorTransform(Vector(1, 0, 0), matrix, transformed);
    passed = std::abs(transformed.x) < 1e-4f && std::abs(transformed.y - 1) < 1e-4f;
    logCheck("mathlib", passed); all &= passed;
    void* memory = g_pMemAlloc->Alloc(128);
    passed = memory && g_pMemAlloc->GetSize(memory) >= 128;
    if (memory) g_pMemAlloc->Free(memory);
    logCheck("allocator", passed); all &= passed;
    const auto timer = Plat_FloatTime();
    passed = timer >= 0 && std::isfinite(timer) && Plat_FloatTime() >= timer;
    logCheck("timer", passed); all &= passed;
    ConVar* found = console->FindVar("ios_rotation_speed");
    const float previous = rotationSpeed.GetFloat();
    if (found) found->SetValue(45.0f);
    passed = found == &rotationSpeed && std::abs(rotationSpeed.GetFloat() - 45) < 1e-5f;
    rotationSpeed.SetValue(previous);
    logCheck("ConVar", passed); all &= passed;
    return all;
}
bool SourceBridge::execute(const std::string& input) {
    if (!ready_ || input.size() > 255) return false;
    CCommand args;
    if (!args.Tokenize(input.c_str()) || args.ArgC() < 1) return false;
    if (!std::strcmp(args[0], "source_content_mount") && args.ArgC()==2) return files_.mountContent(args[1]);
    if (!std::strcmp(args[0], "source_content_unmount") && args.ArgC()==2) return files_.unmountContent(args[1]);
    if (!std::strcmp(args[0], "source_selftest")) return selfTest() && files_.selfTest() && systems_.selfTest() && sourceEngineSelfTest() && sourceAssetsSelfTest() && host_.selfTest() && map_.selfTest();
    if (!std::strcmp(args[0], "source_bsp_selftest")) return map_.selfTest();
    if (!std::strcmp(args[0], "source_bsp_reset")) return map_.resetMap();
    if (!std::strcmp(args[0], "source_bsp_terrain")) return map_.demoTerrain();
    if (!std::strcmp(args[0], "source_bsp_materials")) return map_.demoMaterials();
    if (!std::strcmp(args[0], "source_bsp_props")) return map_.demoProps();
    if (!std::strcmp(args[0], "source_props_selftest")) return map_.propsSelfTest();
    if (!std::strcmp(args[0], "source_model_reset")) return map_.resetModel();
    if (!std::strcmp(args[0], "source_anim_pause")) return map_.setAnimationPlaying(false);
    if (!std::strcmp(args[0], "source_anim_resume")) return map_.setAnimationPlaying(true);
    if (!std::strcmp(args[0], "source_anim_play") && args.ArgC()==2) {
        unsigned index=0;const std::string text=args[1];if(text.empty()||text.size()>3)return false;for(char digit:text){if(digit<'0'||digit>'9')return false;index=index*10+unsigned(digit-'0');}return map_.playAnimation(index);
    }
    if (!std::strcmp(args[0], "source_model_load") && args.ArgC()==2) {
        const std::string path=args[1];
        if(path.empty()||path.find("..")!=std::string::npos||path.front()=='/'||path.find('\\')!=std::string::npos||path.rfind("models/",0)!=0)return false;
        return map_.loadModel(path.c_str());
    }
    if (!std::strcmp(args[0], "source_camera_reset")) { map_.resetCamera(); return true; }
    if (!std::strcmp(args[0], "source_physics_reset")) return map_.resetPhysics();
    if (!std::strcmp(args[0], "source_physics_impulse")) return map_.impulsePhysics();
    if (!std::strcmp(args[0], "source_bsp_load") && args.ArgC()==2) {
        const std::string path=args[1];
        if (path.empty() || path.find("..")!=std::string::npos || path.front()=='/' || path.find('\\')!=std::string::npos || path.rfind("maps/",0)!=0) return false;
        return map_.load(path.c_str());
    }
    if (!std::strcmp(args[0], "source_host_selftest")) return host_.selfTest();
    if (!std::strcmp(args[0], "source_assets_selftest")) return sourceAssetsSelfTest();
    if (!std::strcmp(args[0], "source_engine_selftest")) return sourceEngineSelfTest();
    if (!std::strcmp(args[0], "source_app_selftest")) return systems_.selfTest();
    if (!std::strcmp(args[0], "source_fs_selftest")) return files_.selfTest();
    // Game commands still require a real server DLL and map. Expose only the
    // initialized engine-only harness commands in -nogamedll mode.
    if (!std::strcmp(args[0], "source_status")) {
        auto* command = console->FindCommand(args[0]);
        command->Dispatch(args);
        return true;
    }
    if (!std::strcmp(args[0], "ios_rotation_speed")) {
        auto* variable = console->FindVar(args[0]);
        if (args.ArgC() > 1) variable->SetValue(args[1]);
        Msg("%s = %s\n", variable->GetName(), variable->GetString());
        return true;
    }
    Msg("Unknown Source command: %s\n", args[0]);
    return false;
}
void SourceBridge::frame(double seconds) {
    if (!ready_ || !std::isfinite(seconds) || seconds < 0) return;
    elapsed_ += std::min(seconds, 0.1);
    host_.frame(float(std::min(seconds, 0.1)));
    map_.frame(float(std::min(seconds, 0.1)));
    console->ProcessQueuedMaterialThreadConVarSets();
}
std::vector<SourceVertex> SourceBridge::vertices(float aspect) const {
    return ready_ ? map_.vertices(aspect) : std::vector<SourceVertex>{};
}
}
