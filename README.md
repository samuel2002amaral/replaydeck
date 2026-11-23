# ReplayDeck

**The complete instant replay plugin for OBS Studio: multi-camera recording, smooth slow motion, zoom and live graphics.**

ReplayDeck turns OBS Studio into an instant replay system for live sports, eSports, churches, schools and events.

## Download

Get the latest version on the [Releases page](../../releases/latest):

| System | File |
|---|---|
| Windows (installer, recommended) | `replaydeck-<version>-windows-installer.zip` |
| Windows (manual) | `replaydeck-<version>-windows.zip` |
| macOS (Apple Silicon and Intel) | `replaydeck-<version>-macos.zip` |
| Linux (Ubuntu 24.04 and derivatives) | `replaydeck-<version>-linux.zip` |

Requirements: OBS Studio 30 or newer, 64-bit. The Windows installer and the macOS `Instalar.command` detect your OBS version and install the matching build.

## Features

- **Multi-camera recording**: up to 8 sources at once, frame-synced, GPU intra-frame encoding (NVENC, QSV, AMF; H.264 or HEVC) with constant quality, MJPEG on CPU, automatic disk management.
- **Replay**: two outputs (A and B) or both in sync, angle switching, smooth slow motion 0–100% with frame blending, reverse, fast forward, frame by frame, loop, draggable timeline.
- **Events**: Mark In / Mark Out, one-click 5/10/20 s events, multiple angles per event, Play Events with transitions and stingers, MP4 export without re-encoding.
- **Zoom**: zoom and pan on the replay output with mini-map, slider and arrows.
- **GT titles**: imports vMix GT (.gtzip) titles with their animations, title library, full editor, CSV/TXT/JSON presets.
- **Overlays**: 8 overlay channels above the program.
- **Remote control**: vMix-compatible HTTP/TCP API and hotkeys for every function.
- **10 languages**, switchable inside the plugin.

## Building from source

The plugin is built inside the OBS Studio source tree:

```sh
git clone --recursive --branch 33.0.1 https://github.com/obsproject/obs-studio.git
cp -R replaydeck obs-studio/plugins/replaydeck
echo 'add_subdirectory(replaydeck)' >> obs-studio/plugins/CMakeLists.txt
cd obs-studio
cmake --preset windows-x64 -DENABLE_BROWSER=OFF -DENABLE_WEBSOCKET=OFF   # or: macos
cmake --build build_x64 --config RelWithDebInfo --target replaydeck
```

Build it with the OBS version you target: OBS refuses plugins compiled against a newer libobs.
On Linux, `tools/linux-build.sh` builds against the system `libobs-dev`, Qt 6 and FFmpeg packages.

Translations live in `i18n/<language>.txt` (one line per text in `i18n/keys.txt`); `tools/i18n-build.py` generates `data/lang/*.json`.

## License

ReplayDeck is free software under the GNU General Public License, version 2 or later (see [LICENSE](LICENSE)), like OBS Studio itself.
It includes [miniz](third_party/miniz) (MIT license).
