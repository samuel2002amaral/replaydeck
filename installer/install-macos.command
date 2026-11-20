#!/bin/bash
# Instala o ReplayDeck no OBS do usuário atual, escolhendo a versão certa para o OBS instalado
cd "$(dirname "$0")"
DEST="$HOME/Library/Application Support/obs-studio/plugins"
APP="/Applications/OBS.app"
V=$(defaults read "$APP/Contents/Info" CFBundleShortVersionString 2>/dev/null)
M=${V%%.*}
[[ "$M" =~ ^[0-9]+$ ]] || M=99
if (( M < 30 )); then
  echo "O ReplayDeck precisa do OBS Studio 30 ou mais novo (encontrado: $V)."
  echo "ReplayDeck needs OBS Studio 30 or newer (found: $V)."
  read -n 1 -s -r -p "Pressione uma tecla / Press any key"; exit 1
fi
SRC=""
for (( v=M; v>=30; v-- )); do
  if [ -d "versoes/obs-$v/replaydeck.plugin" ]; then SRC="versoes/obs-$v/replaydeck.plugin"; break; fi
done
if [ -z "$SRC" ]; then
  SRC=$(ls -d versoes/obs-*/replaydeck.plugin | sort -V | tail -1)
fi
mkdir -p "$DEST"
rm -rf "$DEST/replaydeck.plugin" "$DEST/obs-instant-replay.plugin"
cp -R "$SRC" "$DEST/"
xattr -dr com.apple.quarantine "$DEST/replaydeck.plugin" 2>/dev/null || true
echo
echo "OBS ${V:-?} → $SRC"
echo "ReplayDeck instalado em: $DEST/replaydeck.plugin"
echo "Abra o OBS: o painel ReplayDeck fica no menu Painéis (Docks)."
echo
read -n 1 -s -r -p "Pressione uma tecla para fechar / Press any key"
