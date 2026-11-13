# plugin-builder

A minimal, **standalone** build system for [VPinballX](https://github.com/vpinball/vpinball)
plugins. It does not require the vpinball source tree — a plugin is just a shared
library with a pure-C ABI, so all you need are the SDK headers (vendored in
[`sdk/plugins/`](sdk/plugins/)) and a compiler.

Use this as a template to build binary plugins for Windows, macOS, and Linux and
distribute them (e.g. from a plugin store).

## Quick start

1. Clone this repo (or use it as a GitHub template).
2. Edit [`plugin.cfg`](plugin.cfg): set your `id`, name, author, link and the
   `[libraries]` filenames. **This is the only file you need to edit to rename
   the plugin** (see [Renaming the plugin](#renaming-the-plugin)).
3. Write your code in [`src/`](src/). Every `.cpp` under it, including subfolders,
   is compiled.
4. Build, install into VPX, enable it, and run a table (see below).

You need CMake 3.20+ and a C++20 compiler (GCC, Clang, or MSVC 2022).

## How a VPX plugin works

The plugin links against **nothing** from the host. At load time VPX passes a
table of function pointers (`MsgPluginAPI`) and your library exports exactly two
symbols:

```c
<PluginId>PluginLoad(uint32_t sessionId, const MsgPluginAPI* api);
<PluginId>PluginUnload();
```

`<PluginId>` is the `id` in [`plugin.cfg`](plugin.cfg). You don't type it in
your code: CMake reads it from `plugin.cfg` and defines `PLUGIN_ID`, and
[`src/plugin_id.h`](src/plugin_id.h) turns that into the `PLUGIN_LOAD` /
`PLUGIN_UNLOAD` export names and the `PLUGIN_ID_STR` string.

From there, everything happens through messages: you broadcast a request to get
an API table (e.g. `VPXPluginAPI`), and subscribe to events (e.g. game start/end).

### The example plugin

[`src/myplugin.cpp`](src/myplugin.cpp) shows the three things most plugins need:

- **VPX API and events**: fetches `VPXPluginAPI` and subscribes to
  `OnGameStart` / `OnGameEnd`.
- **Logging**: `LOGI(...)` / `LOGW(...)` / `LOGE(...)` write to VPX's log via
  `LoggingPlugin.h`. To log from another `.cpp`, add `LPI_USE_CPP();` at file
  scope and the same `#define LOGI LPI_LOGI_CPP` aliases.
- **A user setting**: `MSGPI_BOOL_VAL_SETTING` declares a setting that shows up
  in VPX's plugin settings and is saved by VPX. `MsgPlugin.h` also has INT,
  FLOAT, ENUM, STRING, FOLDER, and FILE variants.

For anything beyond that (scriptable objects for table scripts, controllers,
DMD/segment displays, audio streams, rendering), the plugins that ship with VPX
are the best reference:
[vpinball/plugins](https://github.com/vpinball/vpinball/tree/master/plugins).
`helloworld` and `helloscript` are the smallest.

**Rule for unloading:** every `SubscribeMsg` needs a matching `UnsubscribeMsg`,
and every `GetMsgID` needs a matching `ReleaseMsgID`, in `PLUGIN_UNLOAD`.

## Layout

```
plugin-builder/
├── CMakeLists.txt            # reads plugin.cfg, no vpinball dependency
├── plugin.cfg                # manifest VPX scans: the single source of the plugin's identity
├── LICENSE                   # GPLv3 (matches the SDK headers; see License below)
├── sdk/
│   ├── VPINBALL_COMMIT       # vpinball commit the headers were synced from
│   └── plugins/              # vendored SDK headers (header-only)
│       ├── MsgPlugin.h          # core messaging/settings/threading ABI
│       ├── VPXPlugin.h          # VPX events + VPXPluginAPI (notifications, input, textures, RunScript...)
│       ├── ControllerPlugin.h   # controller/display/audio sources (included by VPXPlugin.h)
│       ├── LoggingPlugin.h      # host logging
│       ├── ScriptablePlugin.h   # expose scriptable objects to table scripts
│       ├── PagedUIPlugin.h      # declarative in-game UI sheets (WIP upstream, unstable)
│       ├── PluginStrings.h      # UTF-8 / native path helpers
│       └── ColorSpace.h         # sRGB helpers for display frames
├── src/
│   ├── plugin_id.h           # PLUGIN_LOAD / PLUGIN_UNLOAD / PLUGIN_ID_STR from plugin.cfg
│   └── myplugin.cpp          # your plugin
├── scripts/package.sh        # build + stage + zip one platform
├── scripts/update-sdk.sh     # re-sync sdk/plugins/ from vpinball
└── .github/workflows/build.yml   # CI matrix: builds all platforms, releases on tag
```

## Build locally

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

CMake prints which `plugin.cfg` entry it is building, e.g.
`-- Plugin: MyPlugin -> plugin-myplugin.so (linux.x64)`.

To produce a ready-to-ship folder + zip for your platform:

```bash
scripts/package.sh linux-x64     # or macos-universal / windows-x64 / linux-aarch64
```

That stages `dist/<id>/` (containing `plugin.cfg` + the built library) and zips
it as `dist/<id lowercase>-<slug>.zip`.

## Installing and testing in VPX

Copy the `dist/<id>/` folder (or `build/` output + `plugin.cfg` in a folder named
after the id) into one of the folders VPX scans for plugins:

| where                         | path                                                  |
|-------------------------------|-------------------------------------------------------|
| next to the VPX executable    | `<VPX folder>/plugins/`                               |
| macOS app bundle              | `<VPX app>.app/Contents/PlugIns/`                     |
| user preferences folder       | `<VPX prefs folder>/plugins/` (used only if it exists) |

**Plugins are disabled by default.** Enable yours in VPX's plugin settings, or
in `VPinballX.ini`:

```ini
[Plugin.MyPlugin]
Enable = 1
```

VPX loads enabled plugins when a table starts. Check VPX's log for your
`LOGI` lines. If the plugin doesn't appear at all, check that:

- the folder contains `plugin.cfg` and the library named in `[libraries]` for
  your platform,
- the `id` in `plugin.cfg` matches the exports (it does automatically if you
  built with this CMake),
- the SDK headers match your VPX build (see below).

## Renaming the plugin

Edit [`plugin.cfg`](plugin.cfg) only:

1. `id`: must be a valid C identifier (letters, digits, `_`), because it
   becomes the `<id>PluginLoad` export prefix. CMake stops with an error if it
   isn't.
2. The `[libraries]` filenames, following the naming rules below.
3. `name`, `description`, `author`, `version`, `link`.

CMake, the export names, the install folder, `package.sh` and CI artifact names
all follow automatically. You may also want to rename `src/myplugin.cpp`. Its
filename doesn't matter.

## Keeping the SDK in sync with VPX

The headers in `sdk/plugins/` must match the VPX build you target. Refresh them
from vpinball `master` (or any tag/commit) with:

```bash
scripts/update-sdk.sh            # master
scripts/update-sdk.sh <ref>      # a tag or commit
```

The resolved commit is written to [`sdk/VPINBALL_COMMIT`](sdk/VPINBALL_COMMIT).
Review the diff before committing: upstream may change the API.

Two upstream conventions guard against mismatches:

- **Message names end in a version marker**, e.g. `"GetAPI:1"`,
  `"OnGameStart:1"`. When a message's format changes, its marker is bumped, so
  a plugin built against stale headers simply gets no answer. It won't
  misinterpret data. The host asserts that every `GetMsgID` name has a marker.
- **API tables start with `int version`** (`MsgPluginAPI`, `VPXPluginAPI`,
  `LoggingPluginAPI`). New entry points are appended and the version is bumped.
  Check it before calling a function that was added later.

If your plugin worked once and now `vpxApi` stays `nullptr` on a newer VPX
(the example logs an error when this happens), re-sync the headers and rebuild.

## Distribution (the store side)

VPX scans for a folder containing `plugin.cfg`, so the distribution unit is a
directory that drops into the user's plugins path:

```
MyPlugin/
├── plugin.cfg
└── plugin-myplugin.so        # (or .dll / .dylib for that platform)
```

The `[libraries]` table maps platform/arch → filename. Note the naming rules:

| platform key                   | filename                 |
|--------------------------------|--------------------------|
| `windows.x64`                  | `plugin-<name>64.dll`    |
| `windows.x86`                  | `plugin-<name>.dll`      |
| `linux.x64` / `linux.aarch64`  | `plugin-<name>.so`       |
| `macos.x64` / `macos.arm64`    | `plugin-<name>.dylib`    |

CMake picks the entry for the platform it is building for and names the
library to match: a 64-bit Windows build uses `windows.x64`, a 32-bit one
uses `windows.x86`.

Because Linux and macOS use the **same filename across architectures**, a single
folder holds only one binary per OS. So:

- **Serve a separate download per OS+arch** (`linux-x64`, `linux-aarch64`,
  `windows-x64`), and
- **ship macOS as a universal binary** (one `.dylib` with both `x86_64` and
  `arm64`). The CMakeLists does this automatically, and checks that
  `macos.x64` and `macos.arm64` name the same file.

Only Windows encodes the arch in the filename, so x86 + x64 can coexist.

### Platform compatibility

- **C++ runtime.** The API boundary is C (ABI-stable across compilers), but
  your code is C++. On Linux and Windows the template **statically links the
  C++ runtime** (`-static-libgcc -static-libstdc++` with GCC/MinGW, `/MT` with
  MSVC), so the shipped binary doesn't depend on the user's runtime version. On
  Linux those runtime symbols are also kept private (`--exclude-libs,ALL`), so
  only the two plugin entry points are exported. macOS uses the system libc++,
  which is always present.
- **macOS:** the minimum version is set to **14.0**, the same as VPX itself.
  Override with `-DCMAKE_OSX_DEPLOYMENT_TARGET=<ver>`.
- **Linux:** the binary needs the glibc version of the machine it was built on
  or newer. CI builds on Ubuntu 22.04, so users need glibc 2.35+. Build on an
  older distro if you need to support older systems.

## CI / releasing

[`.github/workflows/build.yml`](.github/workflows/build.yml) builds all four
targets on native runners:

| slug              | runner              |
|-------------------|---------------------|
| `linux-x64`       | `ubuntu-22.04`      |
| `linux-aarch64`   | `ubuntu-22.04-arm`  |
| `windows-x64`     | `windows-latest`    |
| `macos-universal` | `macos-14`          |

Every push uploads per-platform zips (`<id lowercase>-<slug>`) as build
artifacts. Pushing a `vX.Y.Z` tag also collects them into a GitHub Release.
Those zips are what your store serves.

```bash
git tag v1.0.0 && git push origin v1.0.0
```

## License

This template is licensed under the GPLv3 ([`LICENSE`](LICENSE)). The SDK
headers under `sdk/plugins/` are `GPLv3+` (copied from vpinball), and the
plugins that ship with VPX are GPLv3+ too. A plugin built from this template
includes those headers, so take that into account when you choose your plugin's
license. If you're unsure, GPLv3 is the safe choice.
