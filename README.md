# plugin-builder

A minimal, **standalone** build system for [VPinballX](https://github.com/vpinball/vpinball)
plugins. It does not require the vpinball source tree — a plugin is just a shared
library with a pure-C ABI, so all you need are the SDK headers (vendored in
[`sdk/plugins/`](sdk/plugins/)) and a compiler.

Use this as a template to build binary plugins for Windows, macOS, and Linux and
distribute them (e.g. from a plugin store).

## How a VPX plugin works

The plugin links against **nothing** from the host. At load time VPX passes a
table of function pointers (`MsgPluginAPI`) and your library exports exactly two
symbols:

```c
<PluginId>PluginLoad(uint32_t sessionId, const MsgPluginAPI* api);
<PluginId>PluginUnload();
```

`<PluginId>` must equal the `id` in [`plugin.cfg`](plugin.cfg). Here the id is
`MyPlugin`, so the exports are `MyPluginPluginLoad` / `MyPluginPluginUnload`
(see [`src/myplugin.cpp`](src/myplugin.cpp)).

## Layout

```
plugin-builder/
├── CMakeLists.txt            # ~60 lines, no vpinball dependency
├── plugin.cfg               # manifest VPX scans (id, metadata, per-platform lib names)
├── sdk/plugins/             # vendored SDK headers (header-only)
│   ├── MsgPlugin.h          #   core messaging/settings ABI
│   ├── ControllerPlugin.h   #   included by VPXPlugin.h
│   ├── VPXPlugin.h          #   VPX events + PushNotification, etc.
│   └── LoggingPlugin.h      #   optional host logging
├── src/myplugin.cpp         # your plugin
├── scripts/package.sh       # build + stage + zip one platform
└── .github/workflows/build.yml   # CI matrix: builds all platforms, releases on tag
```

## Build locally

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

To produce a ready-to-ship folder + zip for your platform:

```bash
scripts/package.sh linux-x64     # or macos-universal / windows-x64 / linux-aarch64
```

That stages `dist/MyPlugin/` (containing `plugin.cfg` + the built library) and
zips it. Drop the `MyPlugin/` folder into VPX's plugins directory and enable it
in the plugin settings.

## Renaming the plugin

Change the id/basename in three places (kept in sync deliberately):

1. [`plugin.cfg`](plugin.cfg): `id` and the `[libraries]` filenames.
2. [`CMakeLists.txt`](CMakeLists.txt): `PLUGIN_ID` and `PLUGIN_BASENAME`.
3. [`src/myplugin.cpp`](src/myplugin.cpp): the `<Id>PluginLoad` / `<Id>PluginUnload`
   export names.

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

Because Linux and macOS use the **same filename across architectures**, a single
folder holds only one binary per OS. So:

- **Serve a separate download per OS+arch** (`linux-x64`, `linux-aarch64`,
  `windows-x64`), and
- **ship macOS as a universal binary** (one `.dylib` with both `x86_64` and
  `arm64`) — the CMakeLists does this automatically.

Only Windows encodes the arch in the filename, so x86 + x64 can coexist.

### C++ runtime — important for binary distribution

The API boundary is C (ABI-stable across compilers), but your code is C++. If
libstdc++/libgcc are linked dynamically, the library can fail to load on a
user's machine with an older runtime. This template **statically links the C++
runtime** on every platform (`-static-libgcc -static-libstdc++` on GCC/Clang,
`/MT` on MSVC), so the shipped binary is self-contained.

## CI / releasing

[`.github/workflows/build.yml`](.github/workflows/build.yml) builds all four
targets on native runners:

| slug              | runner              |
|-------------------|---------------------|
| `linux-x64`       | `ubuntu-22.04`      |
| `linux-aarch64`   | `ubuntu-22.04-arm`  |
| `windows-x64`     | `windows-latest`    |
| `macos-universal` | `macos-14`          |

Every push uploads per-platform zips as build artifacts. Pushing a `vX.Y.Z` tag
additionally collects them into a GitHub Release — those zips are what your store
serves.

```bash
git tag v1.0.0 && git push origin v1.0.0
```

## License

The SDK headers under `sdk/plugins/` are `GPLv3+` (copied from vpinball). Keep
that in mind for your plugin's licensing.
