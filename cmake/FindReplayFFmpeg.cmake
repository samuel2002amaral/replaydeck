# Localiza avcodec/avutil do FFmpeg (o mesmo que acompanha o OBS, via obs-deps).
# Aponte CMAKE_PREFIX_PATH para a pasta do obs-deps.
find_path(REPLAY_FFMPEG_INCLUDE_DIRS libavcodec/avcodec.h)
find_library(REPLAY_AVCODEC_LIB NAMES avcodec)
find_library(REPLAY_AVUTIL_LIB NAMES avutil)
find_library(REPLAY_AVFORMAT_LIB NAMES avformat)
set(REPLAY_FFMPEG_LIBRARIES ${REPLAY_AVCODEC_LIB} ${REPLAY_AVUTIL_LIB} ${REPLAY_AVFORMAT_LIB})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ReplayFFmpeg DEFAULT_MSG
  REPLAY_FFMPEG_INCLUDE_DIRS REPLAY_AVCODEC_LIB REPLAY_AVUTIL_LIB)
