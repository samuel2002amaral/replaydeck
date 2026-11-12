#!/bin/bash
# Compila e roda um teste de interface de test/ui/ contra os objetos de tools/linux-build.sh.
# Uso: tools/ui-test.sh test/ui/fit.cpp [argumentos do teste]
# Roda num X virtual (xvfb-run). As imagens saem em /tmp/wiz/*.png (veja cada teste).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-/tmp/fullbuild}"
SRC="$1"; shift
NAME=$(basename "$SRC" .cpp)
mkdir -p /tmp/wiz
cd "$OUT"
objs=$(ls src_*.o _tmp_*.o 2>/dev/null | grep -v plugin-main)
g++ -std=c++17 -fPIC -g -I"$ROOT/src" -I/usr/include/obs $(pkg-config --cflags libobs Qt6Widgets) -c "$ROOT/$SRC" -o /tmp/wiz/$NAME.o
g++ -o /tmp/wiz/$NAME /tmp/wiz/$NAME.o $objs miniz/*.o $(pkg-config --libs libobs libavcodec libavutil libavformat Qt6Widgets Qt6Network) -lobs-frontend-api -lpthread
REPLAY_CONFIG_DIR=/tmp/wiz/cfg-$NAME-$$ timeout 60 xvfb-run -a /tmp/wiz/$NAME "$@"
