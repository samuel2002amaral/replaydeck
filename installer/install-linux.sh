#!/bin/sh
# Instala o ReplayDeck para o usuário atual
set -e
cd "$(dirname "$0")"
DEST="${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio/plugins"
mkdir -p "$DEST"
rm -rf "$DEST/replaydeck" "$DEST/obs-instant-replay"
cp -r replaydeck "$DEST/"
echo "ReplayDeck instalado em $DEST/replaydeck"
echo "Abra o OBS: o painel ReplayDeck fica no menu Painéis (Docks)."
