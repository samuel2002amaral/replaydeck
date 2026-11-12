#!/bin/bash
# Compila o plugin no Linux (testes aqui e a versão Linux no GitHub Actions).
# OPT="-O2" VERSION=1.0.0 BUILD=N para a versão de distribuição.
# Precisa: libobs-dev, obs-frontend-api, Qt6 (Widgets/Network), FFmpeg (libavcodec/format/util), g++.
# Uso: tools/linux-build.sh            (compila só o que mudou)
#      FORCE=1 tools/linux-build.sh    (recompila tudo — use depois de mexer em .hpp)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-/tmp/fullbuild}"
mkdir -p "$OUT/miniz"
cd "$ROOT"
# miniz (para os .gtzip)
for f in third_party/miniz/*.c; do
  o="$OUT/miniz/$(basename "$f" .c).o"
  [ -f "$o" ] || gcc -fPIC -O1 -c "$f" -o "$o" -Ithird_party/miniz -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES
done
MOCS=""
for h in $(grep -l Q_OBJECT src/*.hpp src/gt/*.hpp src/gt/*.cpp 2>/dev/null); do
  b=$(basename $h | tr . _)
  /usr/lib/qt6/libexec/moc $h -o $OUT/moc_$b.cpp
  case $h in *.cpp) ;; *) MOCS="$MOCS $OUT/moc_$b.cpp";; esac
done
CXXF="-std=c++17 -fPIC ${OPT:--O0 -g} -I. -Isrc -Ithird_party/miniz -I/usr/include/obs -DPLUGIN_VERSION=\"${VERSION:-dev}\" -DPLUGIN_BUILD=\"${BUILD:-dev}\" -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES $(pkg-config --cflags libobs libavcodec libavutil libavformat Qt6Widgets Qt6Network)"
objs=""
for f in src/*.cpp src/gt/*.cpp $MOCS; do
  o=$OUT/$(echo $f | tr / _).o
  if [ ! -f $o ] || [ $f -nt $o ] || [ -n "$FORCE" ]; then g++ $CXXF -c $f -o $o -I$OUT; fi
  objs="$objs $o"
done
g++ -shared -o $OUT/replaydeck.so $objs $OUT/miniz/*.o $(pkg-config --libs libobs libavcodec libavutil libavformat Qt6Widgets Qt6Network) -lobs-frontend-api -lpthread
echo BUILD_OK
