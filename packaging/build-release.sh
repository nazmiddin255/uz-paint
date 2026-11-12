#!/bin/bash
# Release fayllarini tayyorlaydi: .deb va .tar.gz
# Builds release files: .deb and .tar.gz  (run from the repository root)
set -e
VERSION="${1:-$(grep -oP 'APP_VERSION\s+"\K[^"]+' src/state.h)}"
ARCH="$(dpkg --print-architecture 2>/dev/null || echo amd64)"
OUT="dist"
rm -rf "$OUT" && mkdir -p "$OUT"

make clean && make

# --- .tar.gz (o'rnatish skriptlari bilan) ---
T="$OUT/uz-paint-$VERSION"
mkdir -p "$T"
cp uz-paint LICENSE README.md "$T/"
cp scripts/ornatish.sh scripts/ochirish.sh "$T/"
chmod +x "$T/ornatish.sh" "$T/ochirish.sh" "$T/uz-paint"
tar -C "$OUT" -czf "$OUT/uz-paint-$VERSION-linux-$ARCH.tar.gz" "uz-paint-$VERSION"
rm -rf "$T"

# --- .deb ---
P="$OUT/deb"
make install DESTDIR="$P" PREFIX=/usr
install -Dm644 LICENSE "$P/usr/share/doc/uz-paint/copyright"
SIZE=$(du -sk "$P/usr" | cut -f1)
mkdir -p "$P/DEBIAN"
cat > "$P/DEBIAN/control" <<EOF
Package: uz-paint
Version: $VERSION
Section: graphics
Priority: optional
Architecture: $ARCH
Depends: libgtk-3-0 (>= 3.22)
Installed-Size: $SIZE
Maintainer: Uz Paint mualliflari
Homepage: https://github.com/nazmiddin255/uz-paint
Description: Linux uçun yengil rasm çiziş dasturi / Lightweight drawing program
 Uz Paint — Linux uçun yengil rasm çiziş dasturi: 9 xil möyqalam,
 23 ta şakl (çizgandan keyin siljitiş, buriş, kattalaştiriş mumkin),
 tanlaş, matn, ölçamni özgartiriş. Interfeys özbekça (yangi alifbo)
 va inglizça. MIT litsenziyasi.
EOF
dpkg-deb --root-owner-group --build "$P" "$OUT/uz-paint_${VERSION}_${ARCH}.deb"
rm -rf "$P"
ls -la "$OUT"
