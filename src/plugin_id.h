// license:GPLv3+
#pragma once

// Plugin identity helpers.
//
// PLUGIN_ID is defined by the build as a bare token (CMake reads it from the
// `id` field in plugin.cfg), so renaming the plugin only means editing
// plugin.cfg. From it we derive the two symbols VPX looks up:
//
//     PLUGIN_LOAD   -> <id>PluginLoad
//     PLUGIN_UNLOAD -> <id>PluginUnload
//
// and PLUGIN_ID_STR, the id as a string literal.

#ifndef PLUGIN_ID
#error "PLUGIN_ID is not defined: build with CMake, which reads it from plugin.cfg"
#endif

#define PLUGIN_CAT_(a, b) a##b
#define PLUGIN_CAT(a, b)  PLUGIN_CAT_(a, b)
#define PLUGIN_STR_(x)    #x
#define PLUGIN_STR(x)     PLUGIN_STR_(x)

#define PLUGIN_LOAD   PLUGIN_CAT(PLUGIN_ID, PluginLoad)
#define PLUGIN_UNLOAD PLUGIN_CAT(PLUGIN_ID, PluginUnload)
#define PLUGIN_ID_STR PLUGIN_STR(PLUGIN_ID)
