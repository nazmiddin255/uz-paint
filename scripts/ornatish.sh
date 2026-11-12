#!/bin/bash
# Uz Paint ni o'rnatish (sudo kerak emas — faqat sizning foydalanuvchingiz uchun)
# Install Uz Paint for the current user (no sudo needed)
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$HOME/.local/bin"
APPS="$HOME/.local/share/applications"
ICONS="$HOME/.local/share/icons/hicolor/256x256/apps"

if [ ! -x "$DIR/uz-paint" ]; then
  echo "❌ 'uz-paint' fayli topilmadi. Skriptni arxiv ichidan ishga tushiring."
  exit 1
fi

mkdir -p "$BIN" "$APPS" "$ICONS"
install -m 755 "$DIR/uz-paint" "$BIN/uz-paint"
"$BIN/uz-paint" --export-icon "$ICONS/uz-paint.png"
# eski (1-versiya) belgi faylini olib tashlaymiz
rm -f "$HOME/.local/share/icons/uz-paint.png"

cat > "$APPS/uz-paint.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Uz Paint
GenericName=Rasm çiziş
GenericName[en]=Drawing program
Comment=Linux uçun yengil rasm çiziş dasturi
Comment[en]=A lightweight drawing program for Linux
Exec=$BIN/uz-paint %f
Icon=$ICONS/uz-paint.png
Terminal=false
Categories=Graphics;2DGraphics;RasterGraphics;
MimeType=image/png;image/jpeg;image/bmp;image/gif;image/webp;image/tiff;
Keywords=paint;draw;drawing;image;rasm;çiziş;
StartupWMClass=uz-paint
EOF

update-desktop-database "$APPS" 2>/dev/null || true
gtk-update-icon-cache -q "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
echo "✅ Uz Paint örnatildi! Menyudan 'Uz Paint' ni qidiring."
echo "✅ Uz Paint installed! Search for 'Uz Paint' in your app menu."
