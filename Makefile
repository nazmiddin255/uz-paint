# Uz Paint — yig'ish fayli
CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -Wno-unused-parameter
PKGS    := gtk+-3.0
PREFIX  ?= /usr
SRC     := src/uz-paint.c
DEPS    := $(wildcard src/*.c src/*.h)

uz-paint: $(DEPS)
	$(CC) $(CFLAGS) $(SRC) -o $@ $$(pkg-config --cflags --libs $(PKGS)) -lm
	strip $@

install: uz-paint
	install -Dm755 uz-paint $(DESTDIR)$(PREFIX)/bin/uz-paint
	install -Dm644 data/uz-paint.desktop $(DESTDIR)$(PREFIX)/share/applications/uz-paint.desktop
	install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/256x256/apps
	./uz-paint --export-icon $(DESTDIR)$(PREFIX)/share/icons/hicolor/256x256/apps/uz-paint.png

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/uz-paint \
	      $(DESTDIR)$(PREFIX)/share/applications/uz-paint.desktop \
	      $(DESTDIR)$(PREFIX)/share/icons/hicolor/256x256/apps/uz-paint.png

clean:
	rm -f uz-paint

.PHONY: install uninstall clean
