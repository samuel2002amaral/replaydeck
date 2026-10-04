cmake_minimum_required(VERSION 3.22)
project(ReplayDeck VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Build modes:
#  - In-tree build inside OBS Studio source (plugins/replaydeck)
#  - Standalone build against an installed OBS Studio SDK
if(TARGET OBS::libobs)
  set(REPLAY_IN_TREE ON)
  find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
  find_package(FFmpeg REQUIRED avcodec avutil avformat)
  set(REPLAY_OBS_LIBS OBS::libobs OBS::frontend-api)
  set(REPLAY_FFMPEG_LIBS FFmpeg::avcodec FFmpeg::avutil FFmpeg::avformat)
else()
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)
  find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
  find_package(ReplayFFmpeg REQUIRED)
  set(REPLAY_OBS_LIBS OBS::libobs OBS::obs-frontend-api)
  set(REPLAY_FFMPEG_LIBS ${REPLAY_FFMPEG_LIBRARIES})
endif()

add_library(replaydeck MODULE
  src/plugin-main.cpp
  src/engine.cpp src/engine.hpp
  src/recording.cpp src/recording.hpp
  src/camera-recorder.cpp src/camera-recorder.hpp
  src/audio-recorder.cpp src/audio-recorder.hpp
  src/playback-channel.cpp src/playback-channel.hpp
  src/replay-source.cpp
  src/exporter.cpp
  src/replay-dock.cpp src/replay-dock.hpp
  src/setup-wizard.cpp src/setup-wizard.hpp
  src/air-control.cpp src/air-control.hpp
  src/overlay-transition.cpp src/overlay-transition.hpp
  src/overlay-channels.cpp src/overlay-channels.hpp
  src/overlay-dock.cpp src/overlay-dock.hpp
  src/vmix-api.cpp src/vmix-api.hpp
  src/ui-theme.cpp src/ui-theme.hpp
  src/i18n.cpp src/i18n.hpp
  src/gt/gt-doc.cpp src/gt/gt-doc.hpp
  src/gt/gt-render.cpp src/gt/gt-render.hpp
  src/gt/gt-source.cpp src/gt/gt-source.hpp
  src/gt/gt-editor.cpp
  src/gt/gt-dock.cpp src/gt/gt-dock.hpp
  src/gt/gt-convert.cpp
  src/gt/gt-library.cpp
  third_party/miniz/miniz.c third_party/miniz/miniz_zip.c
  third_party/miniz/miniz_tdef.c third_party/miniz/miniz_tinfl.c
  src/session.hpp src/common.hpp)

set(PLUGIN_BUILD "dev" CACHE STRING "Build identifier")
target_compile_definitions(replaydeck PRIVATE
  PLUGIN_VERSION="${PROJECT_VERSION}"
  PLUGIN_BUILD="${PLUGIN_BUILD}"
  MINIZ_NO_ZLIB_COMPATIBLE_NAMES)

target_include_directories(replaydeck PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(NOT REPLAY_IN_TREE)
  target_include_directories(replaydeck PRIVATE ${REPLAY_FFMPEG_INCLUDE_DIRS})
endif()

target_link_libraries(replaydeck PRIVATE
  ${REPLAY_OBS_LIBS} Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network ${REPLAY_FFMPEG_LIBS})

set_target_properties(replaydeck PROPERTIES AUTOMOC ON COMPILE_WARNING_AS_ERROR OFF)

if(MSVC)
  target_compile_options(replaydeck PRIVATE /utf-8 /W3)
  target_compile_definitions(replaydeck PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(replaydeck PRIVATE -Wall -Wextra)
endif()
set_target_properties(replaydeck PROPERTIES PREFIX "")

if(REPLAY_IN_TREE)
  if(APPLE AND COMMAND set_target_properties_obs)
    set_target_properties_obs(replaydeck PROPERTIES FOLDER plugins PREFIX "")
    set_target_properties(replaydeck PROPERTIES
      XCODE_ATTRIBUTE_GCC_TREAT_WARNINGS_AS_ERRORS NO
      XCODE_ATTRIBUTE_SWIFT_TREAT_WARNINGS_AS_ERRORS NO)
  endif()
  return()
endif()

if(WIN32)
  install(TARGETS replaydeck LIBRARY DESTINATION obs-plugins/64bit RUNTIME DESTINATION obs-plugins/64bit)
  install(DIRECTORY data/ DESTINATION data/obs-plugins/replaydeck)
else()
  install(TARGETS replaydeck LIBRARY DESTINATION lib/obs-plugins)
  install(DIRECTORY data/ DESTINATION share/obs/obs-plugins/replaydeck)
endif()
