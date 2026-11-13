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
// <PluginId> is the `id` field in plugin.cfg. You don't type it here: the
// build defines PLUGIN_ID from plugin.cfg and plugin_id.h turns it into the
// PLUGIN_LOAD / PLUGIN_UNLOAD export names.
//
// This example shows the three things most plugins need:
//   1. Getting the VPX API and reacting to game start/end events
//   2. Logging through the host's log (shows up in VPX's log file)
//   3. A user setting, editable in VPX's plugin settings and persisted by VPX
//
// For more (scriptable objects, controllers, displays, audio...) see the
// plugins shipped with VPX: https://github.com/vpinball/vpinball/tree/master/plugins

#include "plugin_id.h"

#include "plugins/MsgPlugin.h"
#include "plugins/VPXPlugin.h"
#include "plugins/LoggingPlugin.h"

#include <string>

using namespace std::string_literals;

namespace {

const MsgPluginAPI* msgApi = nullptr;
VPXPluginAPI* vpxApi = nullptr;

uint32_t endpointId = 0;
unsigned int getVpxApiId = 0, onGameStartId = 0, onGameEndId = 0;

// --- Settings ---------------------------------------------------------------
// MSGPI_<TYPE>_VAL_SETTING declares a setting backed by a static variable
// (<name>_Val). VPX fills it from the saved value, or the default, when it is
// registered. See MsgPlugin.h for INT / FLOAT / ENUM / STRING / FOLDER / FILE.
MSGPI_BOOL_VAL_SETTING(showNotificationProp, "ShowNotification", "Show notification",
   "Show a notification when a game starts", true, true);

} // namespace

// --- Logging ----------------------------------------------------------------
// LPI_IMPLEMENT_CPP defines the logger once; LPISetup() connects it at load.
// Other .cpp files that want to log add `LPI_USE_CPP();` at file scope.
LPI_USE_CPP();
LPI_IMPLEMENT_CPP

#define LOGD LPI_LOGD_CPP
#define LOGI LPI_LOGI_CPP
#define LOGW LPI_LOGW_CPP
#define LOGE LPI_LOGE_CPP

namespace {

void onGameStart(const unsigned int eventId, void* userData, void* eventData)
{
   // The game is starting. After this event, VPX API functions marked
   // "in game only" become callable.
   LOGI("Game started"s);
   if (vpxApi && showNotificationProp_Val)
      vpxApi->PushNotification(PLUGIN_ID_STR " loaded!", 5000);
}

void onGameEnd(const unsigned int eventId, void* userData, void* eventData)
{
   // The game is ending. "In game only" API functions must no longer be called.
   LOGI("Game ended"s);
}

} // namespace

MSGPI_EXPORT void MSGPIAPI PLUGIN_LOAD(const uint32_t sessionId, const MsgPluginAPI* api)
{
   msgApi = api;
   endpointId = sessionId;

   LPISetup(endpointId, msgApi);
   msgApi->RegisterSetting(endpointId, &showNotificationProp);

   // Fetch the VPX API table by broadcasting the "get API" request.
   // Message names carry a version marker (e.g. "GetAPI:1"), so a host whose
   // API is incompatible with these headers simply won't answer and vpxApi
   // stays null. API tables also start with an `int version` field that only
   // grows as entry points are appended.
   msgApi->BroadcastMsg(endpointId, getVpxApiId = msgApi->GetMsgID(VPXPI_NAMESPACE, VPXPI_MSG_GET_API), &vpxApi);
   if (vpxApi == nullptr)
      LOGE("VPX API not available: SDK headers may not match this VPX build"s);

   // Subscribe to the events we care about.
   msgApi->SubscribeMsg(endpointId, onGameStartId = msgApi->GetMsgID(VPXPI_NAMESPACE, VPXPI_EVT_ON_GAME_START), onGameStart, nullptr);
   msgApi->SubscribeMsg(endpointId, onGameEndId = msgApi->GetMsgID(VPXPI_NAMESPACE, VPXPI_EVT_ON_GAME_END), onGameEnd, nullptr);

   LOGI("Plugin loaded"s);
}

MSGPI_EXPORT void MSGPIAPI PLUGIN_UNLOAD()
{
   // Cleanup is mandatory: unsubscribe every callback, then release every msg id.
   msgApi->UnsubscribeMsg(onGameStartId, onGameStart, nullptr);
   msgApi->UnsubscribeMsg(onGameEndId, onGameEnd, nullptr);
   msgApi->ReleaseMsgID(getVpxApiId);
   msgApi->ReleaseMsgID(onGameStartId);
   msgApi->ReleaseMsgID(onGameEndId);
   vpxApi = nullptr;
   msgApi = nullptr;
}
