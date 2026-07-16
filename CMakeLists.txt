cmake_minimum_required(VERSION 3.20)
project(vpx-plugin-builder CXX)

# ---------------------------------------------------------------------------
# Rename these two when you rename your plugin.
#   PLUGIN_ID       must match the `id` field in plugin.cfg AND the export
#                   prefix in your source (<PLUGIN_ID>PluginLoad/Unload).
#   PLUGIN_BASENAME the base of the output filename. VPX convention is
#                   "plugin-<lowercase name>".
# ---------------------------------------------------------------------------
set(PLUGIN_ID       "MyPlugin"     CACHE STRING "Plugin id (matches plugin.cfg + export symbols)")
set(PLUGIN_BASENAME "plugin-myplugin" CACHE STRING "Output library base name")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Add every .cpp under src/ so you can grow the plugin without editing this file.
file(GLOB PLUGIN_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

# A plugin is a shared MODULE. It links nothing from the host.
add_library(plugin MODULE ${PLUGIN_SOURCES})
target_include_directories(plugin PRIVATE "${CMAKE_SOURCE_DIR}/sdk")

# Export only the two PluginLoad/PluginUnload symbols; hide everything else.
set_target_properties(plugin PROPERTIES
   PREFIX ""
   CXX_VISIBILITY_PRESET hidden
   VISIBILITY_INLINES_HIDDEN ON)

# ---------------------------------------------------------------------------
# Per-platform output name + self-contained C++ runtime.
# The API boundary is pure C (ABI-stable), but our code is C++ — so we must not
# depend on the user having our exact libstdc++/libgcc. Static-link it.
# ---------------------------------------------------------------------------
if(WIN32)
   # x64 uses the "64" suffix by VPX convention; x86 would be plain.
   set_target_properties(plugin PROPERTIES OUTPUT_NAME "${PLUGIN_BASENAME}64" SUFFIX ".dll")
   if(MSVC)
      # /MT = statically link the MSVC runtime.
      set_property(TARGET plugin PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
   else() # MinGW
      target_link_options(plugin PRIVATE -static -static-libgcc -static-libstdc++)
      target_link_libraries(plugin ws2_32)
   endif()
elseif(APPLE)
   set_target_properties(plugin PROPERTIES OUTPUT_NAME "${PLUGIN_BASENAME}" SUFFIX ".dylib")
   # Build a universal binary so one .dylib covers macos.x64 AND macos.arm64.
   # Override with -DCMAKE_OSX_ARCHITECTURES=arm64 for a single-arch build.
   if(NOT CMAKE_OSX_ARCHITECTURES)
      set_target_properties(plugin PROPERTIES OSX_ARCHITECTURES "x86_64;arm64")
   endif()
else() # Linux / *nix
   set_target_properties(plugin PROPERTIES OUTPUT_NAME "${PLUGIN_BASENAME}" SUFFIX ".so")
   target_link_options(plugin PRIVATE -static-libgcc -static-libstdc++)
endif()

# ---------------------------------------------------------------------------
# `cmake --install` (or the package step) lays out a ready-to-ship folder:
#   <prefix>/<PLUGIN_ID>/plugin.cfg
#   <prefix>/<PLUGIN_ID>/<the built library>
# Drop that folder into VPX's plugins directory.
# ---------------------------------------------------------------------------
install(TARGETS plugin LIBRARY DESTINATION "${PLUGIN_ID}" RUNTIME DESTINATION "${PLUGIN_ID}")
install(FILES "${CMAKE_SOURCE_DIR}/plugin.cfg" DESTINATION "${PLUGIN_ID}")
