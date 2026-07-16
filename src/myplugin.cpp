// license:GPLv3+
//
// Minimal VPinballX plugin.
//
// A plugin is a plain shared library with a pure-C ABI. It links against
// NOTHING from the host: at load time the host passes a table of function
// pointers (MsgPluginAPI) and the plugin exports exactly two symbols:
//
//     <PluginId>PluginLoad(sessionId, api)
//     <PluginId>PluginUnload()
//
// The <PluginId> prefix MUST match the `id` field in plugin.cfg. Here the id
// is "MyPlugin", so the exports are MyPluginPluginLoad / MyPluginPluginUnload.
// Rename both when you rename the plugin.

#include "plugins/MsgPlugin.h"
#include "plugins/VPXPlugin.h"

namespace MyPlugin {

const MsgPluginAPI* msgApi = nullptr;
VPXPluginAPI* vpxApi = nullptr;

uint32_t endpointId = 0;
unsigned int getVpxApiId = 0, onGameStartId = 0, onGameEndId = 0;

void onGameStart(const unsigned int eventId, void* userData, void* eventData)
{
   // The game is starting. After this event, VPX API functions marked
   // "in game only" become callable.
   if (vpxApi)
      vpxApi->PushNotification("MyPlugin loaded!", 5000);
}

void onGameEnd(const unsigned int eventId, void* userData, void* eventData)
{
   // The game is ending. "In game only" API functions must no longer be called.
}

} // namespace MyPlugin

using namespace MyPlugin;

MSGPI_EXPORT void MSGPIAPI MyPluginPluginLoad(const uint32_t sessionId, const MsgPluginAPI* api)
{
   msgApi = api;
   endpointId = sessionId;

   // Fetch the VPX API table by broadcasting the "get API" request.
   msgApi->BroadcastMsg(endpointId, getVpxApiId = msgApi->GetMsgID(VPXPI_NAMESPACE, VPXPI_MSG_GET_API), &vpxApi);

   // Subscribe to the events we care about.
   msgApi->SubscribeMsg(endpointId, onGameStartId = msgApi->GetMsgID(VPXPI_NAMESPACE, VPXPI_EVT_ON_GAME_START), onGameStart, nullptr);
   msgApi->SubscribeMsg(endpointId, onGameEndId = msgApi->GetMsgID(VPXPI_NAMESPACE, VPXPI_EVT_ON_GAME_END), onGameEnd, nullptr);
}

MSGPI_EXPORT void MSGPIAPI MyPluginPluginUnload()
{
   // Cleanup is mandatory: unsubscribe every callback and release every msg id.
   msgApi->UnsubscribeMsg(onGameStartId, onGameStart, nullptr);
   msgApi->UnsubscribeMsg(onGameEndId, onGameEnd, nullptr);
   msgApi->ReleaseMsgID(getVpxApiId);
   msgApi->ReleaseMsgID(onGameStartId);
   msgApi->ReleaseMsgID(onGameEndId);
   vpxApi = nullptr;
   msgApi = nullptr;
}
