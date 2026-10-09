#include "appframework/IAppSystem.h"
#include "tier0/icommandline.h"
#include "inputsystem/iinputsystem.h"
#include "filesystem.h"
#include "vphysics_interface.h"
#include "SDL.h"
#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <vector>

static std::vector<CreateInterfaceFn> factories;
static void *lookup(const char *name, int *status) {
    for (auto factory : factories) {
        int result = 1;
        if (void *value = factory(name, &result)) {
            if (status) *status = result;
            return value;
        }
    }
    if (status) *status = 1;
    return nullptr;
}
static bool finger(IInputSystem *input, bool expectMotion) {
    SDL_Event event{};
    event.type = SDL_FINGERDOWN;
    event.tfinger.fingerId = INT64_MAX;
    event.tfinger.touchId = 1;
    event.tfinger.x = event.tfinger.y = 0.5f;
    SDL_PushEvent(&event);
    event.type = SDL_FINGERMOTION;
    event.tfinger.dx = 0.125f;
    event.tfinger.dy = -0.25f;
    SDL_PushEvent(&event);
    float x = 0, y = 0;
    if (!input->GetTouchAccumulators(0, x, y)) return false;
    bool ok = std::fabs(x - (expectMotion ? 0.125f : 0.f)) < 1e-6f &&
              std::fabs(y - (expectMotion ? -0.25f : 0.f)) < 1e-6f;
    event.type = SDL_FINGERUP;
    SDL_PushEvent(&event);
    return ok;
}
int main(int argc, char **argv) {
    if (argc != 7) return 2;
    const char *versions[] = {"VEngineCvar004", "VFileSystem022", "InputSystemVersion001",
                             "VPhysics031", "VSoundEmitter002", "SceneFileCache002"};
    for (int i = 1; i < argc; ++i) {
        void *handle = dlopen(argv[i], RTLD_NOW | RTLD_LOCAL);
        if (!handle) { std::fprintf(stderr, "%s\n", dlerror()); return 1; }
        auto factory = reinterpret_cast<CreateInterfaceFn>(dlsym(handle, "CreateInterface"));
        if (!factory) return 1;
        factories.push_back(factory);
    }
    CommandLine()->CreateCmdLine("clientmod-lifecycle -nojoy");
    if (SDL_Init(SDL_INIT_EVENTS) != 0) return 1;
    std::vector<IAppSystem *> systems;
    for (auto version : versions) {
        auto system = static_cast<IAppSystem *>(lookup(version, nullptr));
        if (!system || !system->Connect(lookup)) {
            std::fprintf(stderr, "Connect failed: %s\n", version); return 1;
        }
        systems.push_back(system);
    }
    for (std::size_t i = 0; i < systems.size(); ++i) {
        if (systems[i]->Init() != INIT_OK) {
            std::fprintf(stderr, "Init failed: %s\n", versions[i]); return 1;
        }
    }
    auto input = static_cast<IInputSystem *>(lookup("InputSystemVersion001", nullptr));
    if (!finger(input, true)) return 1;
    auto physics = static_cast<IPhysics *>(lookup("VPhysics031", nullptr));
    auto environment = physics->CreateEnvironment();
    if (!environment || physics->GetActiveEnvironmentByIndex(0) != environment) return 1;
    environment->Simulate(1.f / 66.f);
    physics->DestroyEnvironment(environment);
    for (auto it = systems.rbegin(); it != systems.rend(); ++it) (*it)->Shutdown();
    if (!finger(input, false)) return 1;
    for (auto it = systems.rbegin(); it != systems.rend(); ++it) (*it)->Disconnect();
    SDL_Quit();
    std::puts("Original cvar/filesystem/input/physics/sound/scene Connect + Init + reverse shutdown: PASS");
    std::puts("Original SDL 64-bit finger delivery and shutdown watcher removal: PASS");
    // This deliberately does not initialize graphical modules or assert a rendered game.
    return 0;
}
