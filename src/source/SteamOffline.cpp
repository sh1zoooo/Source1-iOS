// Typed offline compatibility for a build without the desktop Steam runtime.
// No Steam service or authentication is implemented or reported as available.
#define STEAM_API_EXPORTS
#define VERSION_SAFE_STEAM_API_INTERFACES
#include "tier1/strtools.h"
#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"
ISteamClient* g_pSteamClientGameServer = nullptr;
ISteamClient* SteamClient() { return nullptr; }
bool SteamGameServer_InitSafe(uint32, uint16, uint16, uint16, EServerMode, const char*) { return false; }
HSteamPipe SteamGameServer_GetHSteamPipe() { return 0; }
HSteamUser SteamGameServer_GetHSteamUser() { return 0; }
void SteamGameServer_RunCallbacks() {}
void SteamGameServer_Shutdown() {}
void SteamAPI_SetTryCatchCallbacks(bool) {}
void SteamAPI_SetBreakpadAppID(uint32) {}
extern "C" int SteamGameServer_GetIPCCallCount() { return 0; }
