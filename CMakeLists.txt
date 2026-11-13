cmake_minimum_required(VERSION 3.20)

# Match VPX's own minimum macOS version. Must be set before project().
# Override with -DCMAKE_OSX_DEPLOYMENT_TARGET=<ver> if you need to.
set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS version")

project(vpx-plugin-builder CXX)

# ---------------------------------------------------------------------------
# plugin.cfg is the single source of truth for the plugin's identity:
#   [configuration] id    -> PLUGIN_ID (export prefix, install folder)
#   [libraries]     <key> -> output library filename for this platform/arch
# Nothing else needs editing when you rename the plugin.
# ---------------------------------------------------------------------------
set(PLUGIN_CFG "${CMAKE_SOURCE_DIR}/plugin.cfg")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PLUGIN_CFG}")
file(STRINGS "${PLUGIN_CFG}" _cfg_lines)

# Return the quoted value of `key = "value"` from plugin.cfg (empty if absent).
function(plugin_cfg_get key out)
   set(_val "")
   string(REPLACE "." "\\." _key_re "${key}")
   foreach(_line IN LISTS _cfg_lines)
      if(_line MATCHES "^[ \t]*${_key_re}[ \t]*=[ \t]*\"([^\"]*)\"")
         set(_val "${CMAKE_MATCH_1}")
         break()
      endif()
   endforeach()
   set(${out} "${_val}" PARENT_SCOPE)
endfunction()

plugin_cfg_get(id PLUGIN_ID)
if(NOT PLUGIN_ID MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
   message(FATAL_ERROR "plugin.cfg: id \"${PLUGIN_ID}\" must be a valid C identifier "
                       "(it becomes the <id>PluginLoad / <id>PluginUnload export prefix).")
endif()

# Pick the [libraries] key VPX will look up on this platform/arch.
if(WIN32)
   if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(PLUGIN_LIB_KEY "windows.x64")
   else()
      set(PLUGIN_LIB_KEY "windows.x86")
   endif()
elseif(APPLE)
   set(PLUGIN_LIB_KEY "macos.arm64") # universal binary: macos.x64 must name the same file
else()
   if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
      set(PLUGIN_LIB_KEY "linux.aarch64")
   else()
      set(PLUGIN_LIB_KEY "linux.x64")
   endif()
endif()

plugin_cfg_get(${PLUGIN_LIB_KEY} PLUGIN_LIB_FILE)
if(NOT PLUGIN_LIB_FILE)
   message(FATAL_ERROR "plugin.cfg: no [libraries] entry for \"${PLUGIN_LIB_KEY}\".")
endif()
if(APPLE)
   plugin_cfg_get(macos.x64 _mac_x64)
   if(_mac_x64 AND NOT _mac_x64 STREQUAL PLUGIN_LIB_FILE)
      message(FATAL_ERROR "plugin.cfg: macos.x64 and macos.arm64 must name the same file (universal binary).")
   endif()
endif()
get_filename_component(PLUGIN_LIB_NAME "${PLUGIN_LIB_FILE}" NAME_WLE)
get_filename_component(PLUGIN_LIB_EXT  "${PLUGIN_LIB_FILE}" LAST_EXT)
message(STATUS "Plugin: ${PLUGIN_ID} -> ${PLUGIN_LIB_FILE} (${PLUGIN_LIB_KEY})")

# macOS: build a universal binary so one .dylib covers macos.x64 AND macos.arm64.
# Set globally (not just on the plugin target) so dependencies built from
# source, e.g. with FetchContent, are universal too.
# Override with -DCMAKE_OSX_ARCHITECTURES=arm64 for a single-arch build.
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
   set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Every .cpp under src/ (including subfolders), so you can grow the plugin
# without editing this file.
file(GLOB_RECURSE PLUGIN_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

# A plugin is a shared MODULE. It links nothing from the host.
add_library(plugin MODULE ${PLUGIN_SOURCES})
target_include_directories(plugin PRIVATE "${CMAKE_SOURCE_DIR}/sdk" "${CMAKE_SOURCE_DIR}/src")

# PLUGIN_ID is passed as a bare token so src/plugin_id.h can build the export
# names (<id>PluginLoad / <id>PluginUnload) and the id string from it.
target_compile_definitions(plugin PRIVATE PLUGIN_ID=${PLUGIN_ID})

# ---------------------------------------------------------------------------
# Third-party dependencies (see "Dependencies" in the README).
# Preferred: build them from source and link statically, e.g.
#
#   include(FetchContent)
#   FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt GIT_TAG 11.0.2)
#   set(BUILD_SHARED_LIBS OFF)
#   set(CMAKE_POSITION_INDEPENDENT_CODE ON)   # static libs go into a shared plugin
#   FetchContent_MakeAvailable(fmt)
#   target_link_libraries(plugin PRIVATE fmt::fmt)
# ---------------------------------------------------------------------------

# Export only the two PluginLoad/PluginUnload symbols; hide everything else.
set_target_properties(plugin PROPERTIES
   PREFIX ""
   OUTPUT_NAME "${PLUGIN_LIB_NAME}"
   SUFFIX "${PLUGIN_LIB_EXT}"
   CXX_VISIBILITY_PRESET hidden
   VISIBILITY_INLINES_HIDDEN ON)

# ---------------------------------------------------------------------------
# Self-contained C++ runtime.
# The API boundary is pure C (ABI-stable), but our code is C++ — so we must not
# depend on the user having our exact libstdc++/libgcc. Static-link it.
# ---------------------------------------------------------------------------
if(WIN32)
   if(MSVC)
      # /MT = statically link the MSVC runtime.
      set_property(TARGET plugin PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
      target_compile_options(plugin PRIVATE /utf-8 /Zc:__cplusplus)
   else() # MinGW
      target_link_options(plugin PRIVATE -static -static-libgcc -static-libstdc++)
      target_link_libraries(plugin ws2_32)
   endif()
elseif(APPLE)
   # Universal binary: see CMAKE_OSX_ARCHITECTURES near the top.
   # Look for bundled .dylib dependencies next to the plugin.
   set_target_properties(plugin PROPERTIES INSTALL_RPATH "@loader_path" BUILD_WITH_INSTALL_RPATH ON)
else() # Linux / *nix
   # --exclude-libs,ALL keeps the statically linked libstdc++/libgcc symbols
   # private, so they can't clash with the host's own C++ runtime.
   target_link_options(plugin PRIVATE -static-libgcc -static-libstdc++ -Wl,--exclude-libs,ALL)
   # Look for bundled .so dependencies next to the plugin.
   set_target_properties(plugin PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH ON)
endif()

# ---------------------------------------------------------------------------
# `cmake --install` (or the package step) lays out a ready-to-ship folder:
#   <prefix>/<PLUGIN_ID>/plugin.cfg
#   <prefix>/<PLUGIN_ID>/<the built library>
# Drop that folder into VPX's plugins directory.
# ---------------------------------------------------------------------------
install(TARGETS plugin LIBRARY DESTINATION "${PLUGIN_ID}" RUNTIME DESTINATION "${PLUGIN_ID}")
install(FILES "${PLUGIN_CFG}" DESTINATION "${PLUGIN_ID}")
