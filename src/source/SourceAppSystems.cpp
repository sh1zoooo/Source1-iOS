#include "SourceAppSystems.hpp"
#include <string>
#include <vector>
#include "appframework/IAppSystemGroup.h"
#include "filesystem.h"
#include "filesystem/IQueuedLoader.h"
#include "icvar.h"
#include "mathlib/mathlib.h"
#include "tier0/dbg.h"
#include "tier2/tier2.h"
#include "vstdlib/cvar.h"
#include "materialsystem/imaterialsystem.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "istudiorender.h"
#include "vphysics_interface.h"
#include "engine_hlds_api.h"
#include "idedicatedexports.h"

extern CreateInterfaceFn SourceFileSystem_GetFactory();

namespace {
// Native platform front-end required by the real dedicated engine API.
// UIKit owns the loop; SourceHost owns original Host_Init / idle frames; no game server is loaded.
class IOSDedicatedExports final : public CBaseAppSystem<IDedicatedExports> {
public:
    void Sys_Printf(char* text) override { Msg("%s", text); }
    void RunServer() override { Warning("Source iOS: server loop is owned by the UIKit host\n"); }
};
class CoreGroup final : public CAppSystemGroup {
public:
    bool Create() override {
        // Upstream already supports statically linked factories. Keep the
        // actual cvar and stdio interfaces in its dependency/lifetime group.
        const auto cvar = LoadModule(VStdLib_GetICVarFactory());
        const auto filesystem = LoadModule(SourceFileSystem_GetFactory());
        if (!AddSystem(cvar, CVAR_INTERFACE_VERSION)
            || !AddSystem(filesystem, FILESYSTEM_INTERFACE_VERSION)
            || !AddSystem(filesystem, QUEUEDLOADER_INTERFACE_VERSION)) return false;
        const auto modules = LoadModule(Sys_GetFactoryThis());
        auto* materials = static_cast<IMaterialSystem*>(Sys_GetFactoryThis()(MATERIAL_SYSTEM_INTERFACE_VERSION, nullptr));
        if (!materials) return false;
        materials->SetShaderAPI("shaderapiempty");
        AddSystem(&exports_, VENGINE_DEDICATEDEXPORTS_API_VERSION);
        return AddSystem(modules, MATERIAL_SYSTEM_INTERFACE_VERSION)
            && AddSystem(modules, VPHYSICS_INTERFACE_VERSION)
            && AddSystem(modules, DATACACHE_INTERFACE_VERSION)
            && AddSystem(modules, STUDIO_RENDER_INTERFACE_VERSION)
            && AddSystem(modules, MDLCACHE_INTERFACE_VERSION)
            && AddSystem(modules, VENGINE_HLDS_API_VERSION);
    }
    bool PreInit() override {
        auto factory = GetFactory();
        ConnectTier2Libraries(&factory, 1);
        tier2Connected_ = true;
        MathLib_Init(2.2f, 2.2f, 0, 2);
        return true;
    }
    int Main() override { return 0; } // UIKit owns the frame loop.
    void PostShutdown() override { disconnectTier2(); }
    void Destroy() override { disconnectTier2(); }
    void* lookup(const char* name) { return FindSystem(name); }
    CreateInterfaceFn factory() const { return GetFactory(); }
private:
    IOSDedicatedExports exports_;
    void disconnectTier2() {
        if (tier2Connected_) DisconnectTier2Libraries();
        tier2Connected_ = false;
    }
    bool tier2Connected_ = false;
};

// Failure injection only for lifecycle validation; these are test systems,
// never advertised or used as replacements for engine interfaces.
class ProbeSystem final : public CBaseAppSystem<IAppSystem> {
public:
    ProbeSystem(std::vector<std::string>& events, const char* name)
        : events_(events), name_(name) {}
    bool failConnect = false;
    bool failInit = false;
    bool Connect(CreateInterfaceFn) override { record("connect"); return !failConnect; }
    void Disconnect() override { record("disconnect"); }
    InitReturnVal_t Init() override { record("init"); return failInit ? INIT_FAILED : INIT_OK; }
    void Shutdown() override { record("shutdown"); }
private:
    void record(const char* event) { events_.push_back(name_ + ":" + event); }
    std::vector<std::string>& events_;
    std::string name_;
};
class ProbeGroup final : public CAppSystemGroup {
public:
    ProbeGroup(CAppSystemGroup* parent, std::vector<std::string>& events)
        : CAppSystemGroup(parent), a(events, "A"), b(events, "B") {}
    ProbeSystem a, b;
    bool Create() override { AddSystem(&a, "PortLifecycleA"); AddSystem(&b, "PortLifecycleB"); return true; }
    bool PreInit() override { return true; }
    int Main() override { return 0; }
    void PostShutdown() override {}
    void Destroy() override {}
};
bool lifecycle(CoreGroup& parent, unsigned mode) {
    if (mode) Msg("Source appframework: intentional %s failure for rollback self-test\n",
        mode == 1 ? "connection" : "initialization");
    std::vector<std::string> events;
    ProbeGroup group(&parent, events);
    group.b.failConnect = mode == 1;
    group.b.failInit = mode == 2;
    group.Startup();
    const auto stage = group.GetErrorStage();
    group.Shutdown();
    const std::vector<std::string> success = {"A:connect", "B:connect", "A:init", "B:init",
        "B:shutdown", "A:shutdown", "B:disconnect", "A:disconnect"};
    const std::vector<std::string> connection = {"A:connect", "B:connect", "A:disconnect"};
    const std::vector<std::string> initialization = {"A:connect", "B:connect", "A:init", "B:init",
        "A:shutdown", "B:disconnect", "A:disconnect"};
    return mode == 0 ? stage == CAppSystemGroup::NONE && events == success
        : mode == 1 ? stage == CAppSystemGroup::CONNECTION && events == connection
        : stage == CAppSystemGroup::INITIALIZATION && events == initialization;
}
bool report(const char* name, bool passed) {
    Msg("Source appframework self-test %s: %s\n", name, passed ? "PASS" : "FAIL");
    return passed;
}
}

namespace source1ios {
struct SourceAppSystems::Impl { CoreGroup group; };
SourceAppSystems::SourceAppSystems() = default;
SourceAppSystems::~SourceAppSystems() { stop(); }
bool SourceAppSystems::start() {
    if (impl_) return false;
    impl_ = std::make_unique<Impl>();
    impl_->group.Startup();
    if (impl_->group.GetErrorStage() != CAppSystemGroup::NONE) { stop(); return false; }
    Msg("Source appframework initialized: CAppSystemGroup (cvar, filesystem, headless materials, physics, model cache, dedicated engine API)\n");
    Msg("Source engine app-system connected and initialized; dedicated Host_Init follows core checks.\n");
    if (!selfTest()) { stop(); return false; }
    return true;
}
void SourceAppSystems::stop() {
    if (!impl_) return;
    impl_->group.Shutdown();
    impl_.reset();
    Msg("Source appframework shutdown\n");
}
void* SourceAppSystems::find(const char* name) const {
    return impl_ ? impl_->group.lookup(name) : nullptr;
}
bool SourceAppSystems::selfTest() const {
    if (!impl_) return false;
    auto& group = impl_->group;
    int result = IFACE_FAILED;
    auto factory = group.factory();
    bool passed = factory(CVAR_INTERFACE_VERSION, &result) == find(CVAR_INTERFACE_VERSION)
        && result == IFACE_OK && find(FILESYSTEM_INTERFACE_VERSION)
        && find(BASEFILESYSTEM_INTERFACE_VERSION);
    passed &= !factory("PortMissingInterface", &result) && result == IFACE_FAILED;
    bool all = report("interface lookup", passed);
    all &= report("reverse shutdown", lifecycle(group, 0));
    all &= report("connection rollback", lifecycle(group, 1));
    all &= report("initialization rollback", lifecycle(group, 2));
    all &= report("parent factory restored", factory(CVAR_INTERFACE_VERSION, nullptr) == find(CVAR_INTERFACE_VERSION));
    return all;
}
}
