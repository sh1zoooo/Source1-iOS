#include "SourceAssets.hpp"
#include <cmath>
#include <cstring>
#include <memory>
#include "tier0/dbg.h"
#include "tier1/utlbuffer.h"
#include "tier1/convar.h"
#include "tier3/tier3.h"
#include "icvar.h"
#include "vtf/vtf.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "materialsystem/imaterialsystem.h"
#include "vphysics_interface.h"
#include "vphysics/constraints.h"
#include "gametrace.h"

namespace {
bool report(const char* name, bool passed) {
    Msg("Source assets/physics self-test %s: %s\n", name, passed ? "PASS" : "FAIL");
    return passed;
}
class CacheClient final : public IDataCacheClient {
public:
    bool HandleCacheNotification(const DataCacheNotification_t&) override { return true; }
    bool GetItemName(DataCacheClientID_t, const void*, char*, unsigned) override { return false; }
};
}
namespace source1ios {
bool sourceAssetsSelfTest() {
    bool all = report("initialized dependencies", g_pMaterialSystem && g_pMDLCache
        && g_pStudioRender && g_pDataCache && g_pPhysicsCollision
        && g_pCVar->FindVar("sv_allow_wait_command") && g_pCVar->FindVar("developer"));
    if (!all) return false;
    using Texture = std::unique_ptr<IVTFTexture, decltype(&DestroyVTFTexture)>;
    Texture original(CreateVTFTexture(), DestroyVTFTexture);
    Texture loaded(CreateVTFTexture(), DestroyVTFTexture);
    unsigned char pixels[64];
    for (unsigned i = 0; i < sizeof(pixels); ++i) pixels[i] = static_cast<unsigned char>(i * 3 + 7);
    bool initialized = original && loaded && original->Init(4,4,1, IMAGE_FORMAT_RGBA8888,0,1);
    CUtlBuffer serialized;
    if (initialized) {
        for (int mip = 0; mip < original->MipCount(); ++mip)
            std::memset(original->ImageData(0,0,mip), 0x80 + mip, 64 >> (2 * mip));
        std::memcpy(original->ImageData(0,0,0), pixels, sizeof(pixels));
    }
    const bool serializedOK = initialized && original->Serialize(serialized);
    const bool loadedOK = serializedOK && loaded->Unserialize(serialized);
    if (!loadedOK) return report("VTF serialize/read pixels", false);
    all &= report("VTF serialize/read pixels", loadedOK && loaded->Width() == 4 && loaded->Height() == 4
        && loaded->MipCount() == 3 && !std::memcmp(loaded->ImageData(0,0,0), pixels, sizeof(pixels))
        && !std::memcmp(loaded->ImageData(0,0,1), original->ImageData(0,0,1),16)
        && !std::memcmp(loaded->ImageData(0,0,2), original->ImageData(0,0,2),4));
    CUtlBuffer truncated(serialized.Base(), 7, CUtlBuffer::READ_ONLY);
    Texture invalid(CreateVTFTexture(), DestroyVTFTexture);
    Msg("Source VTF: intentional truncated input for rejection self-test\n");
    all &= report("VTF truncated header rejected", invalid && !invalid->Unserialize(truncated));
    bool converted = false;
    if (loadedOK) {
        loaded->ConvertImageFormat(IMAGE_FORMAT_BGRA8888, false);
        const auto* data = loaded->ImageData(0,0,0);
        converted = data && data[0] == pixels[2] && data[1] == pixels[1]
            && data[2] == pixels[0] && data[3] == pixels[3];
    }
    all &= report("VTF RGBA to BGRA", converted);
    CacheClient client;
    auto* section = g_pDataCache->AddSection(&client, "IOSProbe", DataCacheLimits_t(), true);
    int payload = 0x51a7;
    DataCacheHandle_t handle = DC_INVALID_HANDLE;
    const bool added = section && section->Add(42, &payload, sizeof(payload), &handle);
    void* locked = added ? section->Lock(handle) : nullptr;
    all &= report("data cache find/lock", added && section->Find(42) == handle && locked == &payload);
    const bool protectedItem = locked && section->Remove(handle) == DC_LOCKED;
    if (locked) section->Unlock(handle);
    const void* removed = nullptr;
    const bool removedOK = added && section->Remove(handle, &removed) == DC_OK && removed == &payload;
    all &= report("data cache unlock/remove", protectedItem && removedOK && !section->Find(42));
    if (section) g_pDataCache->RemoveSection(section);
    auto* shape = g_pPhysicsCollision->BBoxToCollide(Vector(-8,-8,-8), Vector(8,8,8));
    all &= report("physics box collision", shape != nullptr);
    trace_t trace{};
    if (shape) {
        Ray_t ray;
        ray.Init(Vector(-32,0,0), Vector(32,0,0));
        g_pPhysicsCollision->TraceBox(ray, shape, Vector(0,0,0), QAngle(0,0,0), &trace);
        g_pPhysicsCollision->DestroyCollide(shape);
    }
    all &= report("physics ray trace", shape && std::abs(trace.fraction - .375f) < .01f && !trace.startsolid);
    auto* physics = static_cast<IPhysics*>(Sys_GetFactoryThis()(VPHYSICS_INTERFACE_VERSION, nullptr));
    auto* environment = physics ? physics->CreateEnvironment() : nullptr;
    bool simulated = false;
    if (environment) {
        environment->SetGravity(Vector(0,0,-600));
        objectparams_t params{nullptr,1,1,0,0,.05f,"iOS physics probe",nullptr,0,1,true};
        auto* sphere = environment->CreateSphereObject(1, 0, Vector(0,0,64), QAngle(0,0,0), &params, false);
        if (sphere) {
            sphere->Wake();
            for (unsigned i = 0; i < 10; ++i) environment->Simulate(.01f);
            Vector position;
            sphere->GetPosition(&position, nullptr);
            simulated = std::isfinite(position.z) && position.z < 63.5f && position.z > 50;
            environment->DestroyObject(sphere);
        }
        physics->DestroyEnvironment(environment);
    }
    all &= report("physics gravity simulation", simulated);
    // Exercise the actual Havana ragdoll solver, not only unconstrained bodies.
    // A fixed reference and a moving body share a joint with three angular limits.
    environment = physics ? physics->CreateEnvironment() : nullptr;
    bool constrained = false;
    if (environment) {
        environment->SetGravity(Vector(0,0,-600));
        objectparams_t params{nullptr,1,1,0,0,.05f,"iOS ragdoll probe",nullptr,0,1,true};
        auto* anchor = environment->CreateSphereObject(1,0,Vector(0,0,64),QAngle(0,0,0),&params,true);
        auto* body = environment->CreateSphereObject(1,0,Vector(0,0,56),QAngle(0,0,0),&params,false);
        IPhysicsConstraint* joint = nullptr;
        if (anchor && body) {
            constraint_ragdollparams_t limits;
            limits.Defaults();
            MatrixSetColumn(Vector(0,0,-4),3,limits.constraintToReference);
            MatrixSetColumn(Vector(0,0,4),3,limits.constraintToAttached);
            for (auto& axis : limits.axes) axis.SetAxisFriction(-45,45,0);
            joint = environment->CreateRagdollConstraint(anchor,body,nullptr,limits);
            if (joint) {
                body->Wake();
                body->ApplyForceCenter(Vector(250,0,0));
                for (unsigned i = 0; i < 100; ++i) environment->Simulate(.01f);
                Vector referencePoint, attachedPoint, position;
                anchor->LocalToWorld(&referencePoint,Vector(0,0,-4));
                body->LocalToWorld(&attachedPoint,Vector(0,0,4));
                body->GetPosition(&position,nullptr);
                constrained = position.IsValid() && std::abs(position.x) > .01f
                    && (referencePoint-attachedPoint).Length() < .2f;
            }
        }
        if (joint) environment->DestroyConstraint(joint);
        if (body) environment->DestroyObject(body);
        if (anchor) environment->DestroyObject(anchor);
        physics->DestroyEnvironment(environment);
    }
    all &= report("physics ragdoll joint under impulse", constrained);
    if (all) Msg("Source dependencies initialized: materialsystem/shaderapiempty, datacache/MDLCache, studiorender, vphysics/IVP, VTF.\n");
    return all;
}
}
