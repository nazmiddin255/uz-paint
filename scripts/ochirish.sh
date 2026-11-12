#!/bin/bash
# Uz Paint ni kompyuterdan o'chirish / Uninstall Uz Paint
rm -f "$HOME/.local/bin/uz-paint" \
      "$HOME/.local/share/applications/uz-paint.desktop" \
      "$HOME/.local/share/icons/hicolor/256x256/apps/uz-paint.png" \
      "$HOME/.local/share/icons/uz-paint.png"
update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
if [ "$1" = "--hammasi" ] || [ "$1" = "--all" ]; then
  rm -rf "$HOME/.config/uz-paint" "$HOME/.local/share/uz-paint"
  echo "🗑  Sozlamalar ham öçirildi. / Settings removed too."
fi
echo "🗑  Uz Paint öçirildi. / Uz Paint removed."
