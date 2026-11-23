# ReplayDeck Architecture Notes

## Overview
ReplayDeck is structured around a decoupled capture-and-playback core (`src/engine.cpp`) separated from the Qt6 frontend docks (`src/replay-dock.cpp`, `src/overlay-dock.cpp`, `src/gt/gt-dock.cpp`).

## Dual Playback Channels (A / B)
- **Independent & Synced Modes:** Channel A and Channel B maintain separate read cursors over recorded camera segments (`playback-channel.cpp`). In `SEL_AB` mode, transport commands (play, pause, speed, frame step) apply synchronously to both channels while allowing independent camera angle selection.
- **OBS Source Integration:** `replay-source.cpp` exposes the active playback output as a native `obs_source_t`, avoiding blocking disk reads on the OBS video render thread.

## Capture & Export Pipeline
- **Multi-Camera Recording:** `camera-recorder.cpp` and `audio-recorder.cpp` buffer incoming frames per angle so Mark In / Mark Out and fixed-duration event triggers (5s, 10s, 20s) can slice segments without interrupting ongoing capture.
- **FFmpeg Export:** `exporter.cpp` remuxes or transcodes marked event ranges directly to disk using `libavformat` and `libavcodec`.

## Threading & Memory Management
- UI actions triggered from OBS frontend hotkeys or the HTTP/TCP control server (`vmix-api.cpp`) marshal state changes to the Qt main thread via `QMetaObject::invokeMethod` (`Qt::QueuedConnection`).
- OBS references (`obs_source_t`, `obs_data_t`, `obs_data_array_t`) are released immediately after use and cleaned up during `OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP` and `OBS_FRONTEND_EVENT_EXIT`.
