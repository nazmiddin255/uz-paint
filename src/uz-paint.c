/*
 * Uz Paint — Linux uchun yengil rasm chizish dasturi.
 * Copyright (c) 2026 Uz Paint mualliflari
 * MIT litsenziyasi asosida tarqatiladi (LICENSE fayliga qarang).
 *
 * Bu fayl qolgan qismlarni bitta dasturga yig'adi:
 *   state.h   — umumiy turlar va holat
 *   i18n.c    — o'zbekcha (yangi alifbo) va inglizcha matnlar
 *   shapes.c  — 23 ta shakl va barcha belgilar (ikonlar)
 *   core.c    — kanvas, obyektlar, sichqoncha (brushes.c ni ham ulaydi)
 *   ops.c     — fayllar, rasm amallari, bufer, chop etish
 *   ui.c      — menyular, lenta, mavzu, til va main()
 *
 * Yig'ish:  make   (yoki)
 *   gcc -O2 uz-paint.c -o uz-paint $(pkg-config --cflags --libs gtk+-3.0) -lm
 */
#include "state.h"
#include <stdio.h>
#include "i18n.c"
#include "shapes.c"
#include "core.c"
#include "ops.c"
#include "ui.c"
