#include "IShaderSystem.h"
#include "tier1/interface.h"
#include <iostream>
#include <set>
#include <string>

int main() {
    auto *registry = static_cast<IShaderDLLInternal *>(
        Sys_GetFactoryThis()(SHADER_DLL_INTERFACE_VERSION, nullptr));
    if (!registry || registry->ShaderCount() < 75) {
        std::cerr << "Original ClientMod shader registry is missing or incomplete\n";
        return 1;
    }
    std::set<std::string> names;
    for (int i = 0; i < registry->ShaderCount(); ++i) {
        auto *shader = registry->GetShader(i);
        if (!shader || !shader->GetName() || !*shader->GetName()) return 1;
        names.insert(shader->GetName());
    }
    for (const char *required : {"VertexLitGeneric", "LightmappedGeneric",
             "WorldVertexTransition_DX9", "Water_DX90", "Refract_DX90", "Sky_HDR_DX9"}) {
        if (!names.count(required)) {
            std::cerr << "Missing original shader family: " << required << '\n';
            return 1;
        }
    }
    if (registry->GetShader(-1) || registry->GetShader(registry->ShaderCount())) return 1;
    std::cout << "Original ClientMod ShaderDLL004: " << registry->ShaderCount()
              << " shader registrations; required world/model/sky/water families: PASS\n"
              << "Registry only; GPU drawing is not exercised.\n";
}
