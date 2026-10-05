#include "SourceEngine.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "tier0/dbg.h"
#include "tier1/convar.h"
#include "icvar.h"
class CAppSystemGroup;
#include "engine_hlds_api.h"
#include "basehandle.h"
#include "ispatialpartition.h"
#include "engine/IEngineTrace.h"
#include "gametrace.h"
#include "cmd.h"

// Original engine entry points, not substitute implementations.
extern ISpatialPartition* CreateSpatialPartition(const Vector&, const Vector&);
extern void DestroySpatialPartition(ISpatialPartition*);
extern bool host_initialized;

namespace {
bool report(const char* name, bool passed) {
    Msg("Source engine self-test %s: %s\n", name, passed ? "PASS" : "FAIL");
    return passed;
}
std::vector<std::string>* commandResults = nullptr;
void probeCommand(const CCommand& args) {
    if (commandResults && args.ArgC() == 2) commandResults->emplace_back(args[1]);
}
ConCommand engineProbe("ios_engine_probe", probeCommand, "Isolated upstream engine command test");
// Test objects represent bounded entities; they do not replace engine interfaces.
class BoundsEntity final : public IHandleEntity {
public:
    void SetRefEHandle(const CBaseHandle& value) override { handle_ = value; }
    const CBaseHandle& GetRefEHandle() const override { return handle_; }
private:
    CBaseHandle handle_;
};
class Results final : public IPartitionEnumerator {
public:
    IterationRetval_t EnumElement(IHandleEntity* value) override {
        entities.push_back(value);
        return ITERATION_CONTINUE;
    }
    bool only(IHandleEntity* value) const { return entities.size() == 1 && entities[0] == value; }
    std::vector<IHandleEntity*> entities;
};
}

namespace source1ios {
bool sourceEngineSelfTest() {
    int result = IFACE_FAILED;
    auto factory = Sys_GetFactoryThis();
    bool all = report("factory", factory(VENGINE_HLDS_API_VERSION, &result)
        && result == IFACE_OK && factory(INTERFACEVERSION_SPATIALPARTITION, nullptr));
    std::vector<std::string> commands;
    commandResults = &commands;
    {
        auto& probe = engineProbe;
        if (!g_pCVar->FindCommand(probe.GetName())) g_pCVar->RegisterConCommand(&probe);
        Cbuf_Init();
        Cbuf_AddText("ios_engine_probe first; ios_engine_probe \"two words\"\n");
        Cbuf_Execute();
        all &= report("command buffer order/quotes", commands == std::vector<std::string>{"first", "two words"});
        commands.clear();
        Cbuf_AddText("ios_engine_probe before; wait; ios_engine_probe after\n");
        Cbuf_Execute();
        const bool deferred = commands == std::vector<std::string>{"before"};
        Cbuf_Execute();
        all &= report("command buffer wait", deferred && commands == std::vector<std::string>{"before", "after"});
        if (!host_initialized) Cbuf_Shutdown();
        g_pCVar->UnregisterConCommand(&probe);
    }
    commandResults = nullptr;
    std::unique_ptr<ISpatialPartition, decltype(&DestroySpatialPartition)> partition(
        CreateSpatialPartition(Vector(-4096,-4096,-4096), Vector(4096,4096,4096)), DestroySpatialPartition);
    if (!partition) return report("spatial allocation", false);
    BoundsEntity solid, trigger;
    const auto a = partition->CreateHandle(&solid, PARTITION_ENGINE_SOLID_EDICTS,
        Vector(-8,-8,-8), Vector(8,8,8));
    const auto b = partition->CreateHandle(&trigger, PARTITION_ENGINE_TRIGGER_EDICTS,
        Vector(96,-8,-8), Vector(112,8,8));
    Results box;
    partition->EnumerateElementsInBox(PARTITION_ENGINE_SOLID_EDICTS,
        Vector(-16,-16,-16), Vector(16,16,16), false, &box);
    all &= report("spatial box", a != PARTITION_INVALID_HANDLE && b != PARTITION_INVALID_HANDLE && box.only(&solid));
    Results filtered;
    partition->EnumerateElementsInBox(PARTITION_ENGINE_TRIGGER_EDICTS,
        Vector(-128,-128,-128), Vector(128,128,128), false, &filtered);
    all &= report("spatial list masks", filtered.only(&trigger));
    Ray_t ray;
    ray.Init(Vector(-32,0,0), Vector(32,0,0));
    Results hit;
    partition->EnumerateElementsAlongRay(PARTITION_ENGINE_SOLID_EDICTS, ray, false, &hit);
    all &= report("spatial ray", hit.only(&solid));
    partition->ElementMoved(a, Vector(504,-8,-8), Vector(520,8,8));
    Results oldBox, movedBox, deletedBox;
    partition->EnumerateElementsInBox(PARTITION_ENGINE_SOLID_EDICTS,
        Vector(-16,-16,-16), Vector(16,16,16), false, &oldBox);
    partition->EnumerateElementsInSphere(PARTITION_ENGINE_SOLID_EDICTS, Vector(512,0,0), 16, false, &movedBox);
    partition->DestroyHandle(a);
    partition->DestroyHandle(b);
    partition->EnumerateElementsInSphere(PARTITION_ENGINE_SOLID_EDICTS, Vector(512,0,0), 16, false, &deletedBox);
    all &= report("spatial move/delete", oldBox.entities.empty() && movedBox.only(&solid) && deletedBox.entities.empty());
    if (all) Msg("Source engine linked: dedicated engine; command buffer and spatial partition verified.\n");
    return all;
}
}
