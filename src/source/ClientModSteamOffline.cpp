// Offline-only ABI glue. No identity, session, callbacks or authentication are
// supplied. Original UI objects may register callbacks during construction even
// in a NO_STEAM build; no Steam service is present to deliver those callbacks.
#define STEAM_API_EXPORTS
#define VERSION_SAFE_STEAM_API_INTERFACES
#include "tier1/strtools.h"
#include "steam/steam_api.h"
HSteamPipe SteamAPI_GetHSteamPipe() { return 0; }
HSteamUser SteamAPI_GetHSteamUser() { return 0; }
HSteamPipe GetHSteamPipe() { return 0; }
HSteamUser GetHSteamUser() { return 0; }
void SteamAPI_RunCallbacks() {}
void SteamAPI_RegisterCallback(CCallbackBase *, int) {}
void SteamAPI_UnregisterCallback(CCallbackBase *) {}
void SteamAPI_RegisterCallResult(CCallbackBase *, SteamAPICall_t) {}
void SteamAPI_UnregisterCallResult(CCallbackBase *, SteamAPICall_t) {}
