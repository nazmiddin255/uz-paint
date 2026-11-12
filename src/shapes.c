/*
 * Uz Paint — shakllar va belgilar (ikonlar).
 * Barcha shakl va belgilar shu yerda koddan chiziladi (tashqi rasm yo'q).
 * Copyright (c) 2026 Uz Paint mualliflari — MIT litsenziyasi.
 */

/* 0..1 oralig'idagi nuqtalarni (bx,by,bw,bh) qutiga joylab, yopiq yo'l chizadi */
static void unit_poly(cairo_t *cr, double bx, double by, double bw, double bh, const double *p, int n) {
  cairo_move_to(cr, bx + p[0] * bw, by + p[1] * bh);
  for (int i = 1; i < n; i++) cairo_line_to(cr, bx + p[2 * i] * bw, by + p[2 * i + 1] * bh);
  cairo_close_path(cr);
}

/* to'g'ri ko'pburchak yoki yulduz nuqtalari, 0..1 qutiga cho'zilgan */
static int regular_unit(int n, double inner, double *out) {
  int cnt = inner > 0 ? 2 * n : n;
  double minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
  for (int i = 0; i < cnt; i++) {
    double a = -G_PI / 2 + i * (2 * G_PI / cnt);
    double r = (inner > 0 && (i & 1)) ? inner : 1.0;
    out[2 * i] = r * cos(a);
    out[2 * i + 1] = r * sin(a);
    minx = MIN(minx, out[2 * i]); maxx = MAX(maxx, out[2 * i]);
    miny = MIN(miny, out[2 * i + 1]); maxy = MAX(maxy, out[2 * i + 1]);
  }
  for (int i = 0; i < cnt; i++) {
    out[2 * i] = (out[2 * i] - minx) / (maxx - minx);
    out[2 * i + 1] = (out[2 * i + 1] - miny) / (maxy - miny);
  }
  return cnt;
}

static void rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
  r = MIN(r, MIN(w, h) / 2);
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -G_PI / 2, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, G_PI / 2);
  cairo_arc(cr, x + r, y + h - r, r, G_PI / 2, G_PI);
  cairo_arc(cr, x + r, y + r, r, G_PI, 3 * G_PI / 2);
  cairo_close_path(cr);
}

static const double ARROW_R[] = { 0, 0.28, 0.58, 0.28, 0.58, 0, 1, 0.5, 0.58, 1, 0.58, 0.72, 0, 0.72 };
static const double LIGHTNING[] = { 0.56, 0, 0.18, 0.47, 0.44, 0.52, 0.22, 1, 0.84, 0.4, 0.57, 0.36, 0.8, 0 };
static const double ICON_POLY[] = { 0.1, 0.35, 0.5, 0.05, 0.92, 0.3, 0.75, 0.95, 0.3, 0.8 };

/* Shakl yo'lini quti ichida quradi (manfiy o'lcham ham bo'lishi mumkin) */
static void shape_path_box(cairo_t *cr, int kind, double x, double y, double w, double h,
                           const double *poly, int npoly) {
  double bx = MIN(x, x + w), by = MIN(y, y + h), bw = fabs(w), bh = fabs(h);
  if (bw < 0.5 || bh < 0.5) {
    if (kind != SH_LINE && kind != SH_CURVE) return;
  }
  double p[32];
  int n;
  switch (kind) {
  case SH_LINE:
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + w, y + h);
    break;
  case SH_CURVE:
    cairo_move_to(cr, x, y + h);
    cairo_curve_to(cr, x + w * 0.2, y - h * 0.4, x + w * 0.7, y + h * 1.3, x + w, y);
    break;
  case SH_OVAL:
    cairo_save(cr);
    cairo_translate(cr, bx + bw / 2, by + bh / 2);
    cairo_scale(cr, bw / 2, bh / 2);
    cairo_new_sub_path(cr);
    cairo_arc(cr, 0, 0, 1, 0, 2 * G_PI);
    cairo_restore(cr);
    cairo_close_path(cr);
    break;
  case SH_RECT:
    cairo_rectangle(cr, bx, by, bw, bh);
    break;
  case SH_RRECT:
    rounded_rect(cr, bx, by, bw, bh, MIN(bw, bh) * 0.2);
    break;
  case SH_POLYGON:
    if (poly && npoly >= 3) unit_poly(cr, bx, by, bw, bh, poly, npoly);
    else unit_poly(cr, bx, by, bw, bh, ICON_POLY, 5);
    break;
  case SH_TRIANGLE: {
    static const double t[] = { 0.5, 0, 1, 1, 0, 1 };
    unit_poly(cr, bx, by, bw, bh, t, 3);
    break;
  }
  case SH_RTRIANGLE: {
    static const double t[] = { 0, 0, 1, 1, 0, 1 };
    unit_poly(cr, bx, by, bw, bh, t, 3);
    break;
  }
  case SH_DIAMOND: {
    static const double t[] = { 0.5, 0, 1, 0.5, 0.5, 1, 0, 0.5 };
    unit_poly(cr, bx, by, bw, bh, t, 4);
    break;
  }
  case SH_PENTAGON:
    n = regular_unit(5, 0, p);
    unit_poly(cr, bx, by, bw, bh, p, n);
    break;
  case SH_HEXAGON: {
    static const double t[] = { 0, 0.5, 0.25, 0, 0.75, 0, 1, 0.5, 0.75, 1, 0.25, 1 };
    unit_poly(cr, bx, by, bw, bh, t, 6);
    break;
  }
  case SH_ARROW_R: case SH_ARROW_L: case SH_ARROW_U: case SH_ARROW_D:
    for (int i = 0; i < 7; i++) {
      double px = ARROW_R[2 * i], py = ARROW_R[2 * i + 1];
      if (kind == SH_ARROW_L) { px = 1 - px; }
      else if (kind == SH_ARROW_U) { double t = px; px = py; py = 1 - t; }
      else if (kind == SH_ARROW_D) { double t = px; px = py; py = t; }
      p[2 * i] = px; p[2 * i + 1] = py;
    }
    unit_poly(cr, bx, by, bw, bh, p, 7);
    break;
  case SH_STAR4:
    n = regular_unit(4, 0.3, p);
    unit_poly(cr, bx, by, bw, bh, p, n);
    break;
  case SH_STAR5:
    n = regular_unit(5, 0.42, p);
    unit_poly(cr, bx, by, bw, bh, p, n);
    break;
  case SH_STAR6:
    n = regular_unit(6, 0.55, p);
    unit_poly(cr, bx, by, bw, bh, p, n);
    break;
  case SH_CALLOUT_RRECT: {
    double bot = by + bh * 0.76, r = MIN(bw, bh * 0.76) * 0.2;
    cairo_new_sub_path(cr);
    cairo_move_to(cr, bx + r, by);
    cairo_line_to(cr, bx + bw - r, by);
    cairo_arc(cr, bx + bw - r, by + r, r, -G_PI / 2, 0);
    cairo_line_to(cr, bx + bw, bot - r);
    cairo_arc(cr, bx + bw - r, bot - r, r, 0, G_PI / 2);
    cairo_line_to(cr, bx + bw * 0.42, bot);
    cairo_line_to(cr, bx + bw * 0.16, by + bh);
    cairo_line_to(cr, bx + bw * 0.24, bot);
    cairo_line_to(cr, bx + r, bot);
    cairo_arc(cr, bx + r, bot - r, r, G_PI / 2, G_PI);
    cairo_line_to(cr, bx, by + r);
    cairo_arc(cr, bx + r, by + r, r, G_PI, 3 * G_PI / 2);
    cairo_close_path(cr);
    break;
  }
  case SH_CALLOUT_OVAL: {
    double cx = bx + bw / 2, cy = by + bh * 0.4, rx = bw / 2, ry = bh * 0.4;
    double a1 = 100 * G_PI / 180, a2 = 128 * G_PI / 180;
    cairo_new_sub_path(cr);
    cairo_move_to(cr, cx + rx * cos(a2), cy + ry * sin(a2));
    for (double a = a2; a <= a1 + 2 * G_PI + 1e-9; a += G_PI / 60)
      cairo_line_to(cr, cx + rx * cos(a), cy + ry * sin(a));
    cairo_line_to(cr, cx + rx * cos(a1), cy + ry * sin(a1));
    cairo_line_to(cr, bx + bw * 0.14, by + bh);
    cairo_close_path(cr);
    break;
  }
  case SH_CALLOUT_CLOUD: {
    double cx = bx + bw / 2, cy = by + bh * 0.4, rx = bw * 0.39, ry = bh * 0.3;
    const int N = 10;
    cairo_new_sub_path(cr);
    for (int i = 0; i <= N; i++) {
      double a0 = 2 * G_PI * i / N + 0.2, a1 = 2 * G_PI * (i + 1) / N + 0.2;
      double x0 = cx + rx * cos(a0), y0 = cy + ry * sin(a0);
      double x1 = cx + rx * cos(a1), y1 = cy + ry * sin(a1);
      if (i == 0) cairo_move_to(cr, x0, y0);
      if (i == N) break;
      double k = 0.42;
      cairo_curve_to(cr, x0 + (x0 - cx) * k, y0 + (y0 - cy) * k,
                     x1 + (x1 - cx) * k, y1 + (y1 - cy) * k, x1, y1);
    }
    cairo_close_path(cr);
    double s = MIN(bw, bh);
    cairo_new_sub_path(cr);
    cairo_arc(cr, bx + bw * 0.24, by + bh * 0.84, s * 0.055, 0, 2 * G_PI);
    cairo_close_path(cr);
    cairo_new_sub_path(cr);
    cairo_arc(cr, bx + bw * 0.13, by + bh * 0.95, s * 0.035, 0, 2 * G_PI);
    cairo_close_path(cr);
    break;
  }
  case SH_HEART:
    cairo_move_to(cr, bx + 0.5 * bw, by + 0.28 * bh);
    cairo_curve_to(cr, bx + 0.5 * bw, by + 0.0 * bh, bx + 0.0 * bw, by - 0.02 * bh, bx + 0.0 * bw, by + 0.3 * bh);
    cairo_curve_to(cr, bx + 0.0 * bw, by + 0.58 * bh, bx + 0.32 * bw, by + 0.76 * bh, bx + 0.5 * bw, by + 1.0 * bh);
    cairo_curve_to(cr, bx + 0.68 * bw, by + 0.76 * bh, bx + 1.0 * bw, by + 0.58 * bh, bx + 1.0 * bw, by + 0.3 * bh);
    cairo_curve_to(cr, bx + 1.0 * bw, by - 0.02 * bh, bx + 0.5 * bw, by + 0.0 * bh, bx + 0.5 * bw, by + 0.28 * bh);
    cairo_close_path(cr);
    break;
  case SH_LIGHTNING:
    unit_poly(cr, bx, by, bw, bh, LIGHTNING, 7);
    break;
  }
}

/* ---------- belgilar (ikonlar) ---------- */

/* mo'yqalam namunasi: to'lqinsimon chiziq har xil uslubda */
static void icon_brush_sample(cairo_t *cr, int b, const GdkRGBA *fg) {
  cairo_save(cr);
  GRand *r = g_rand_new_with_seed(7 + b);
  #define WAVE() do { cairo_move_to(cr, 3, 17); cairo_curve_to(cr, 8, 3, 14, 23, 21, 7); } while (0)
  switch (b) {
  case B_NORMAL:
    cairo_set_line_width(cr, 3.2); WAVE(); cairo_stroke(cr);
    break;
  case B_CALLIGRAPHY:
    for (double t = 0; t <= 1.0; t += 0.02) {
      double u = 1 - t;
      double px = u*u*u*3 + 3*u*u*t*8 + 3*u*t*t*14 + t*t*t*21;
      double py = u*u*u*17 + 3*u*u*t*3 + 3*u*t*t*23 + t*t*t*7;
      cairo_move_to(cr, px - 2.2, py + 2.2);
      cairo_line_to(cr, px + 2.2, py - 2.2);
    }
    cairo_set_line_width(cr, 1.2); cairo_stroke(cr);
    break;
  case B_PEN:
    cairo_set_line_width(cr, 1.3); WAVE(); cairo_stroke(cr);
    cairo_move_to(cr, 15, 15); cairo_line_to(cr, 21, 21); cairo_line_to(cr, 18, 13); cairo_close_path(cr); cairo_fill(cr);
    break;
  case B_AIRBRUSH:
    for (int i = 0; i < 70; i++) {
      double a = g_rand_double(r) * 2 * G_PI, d = sqrt(g_rand_double(r)) * 8;
      cairo_rectangle(cr, 12 + d * cos(a), 12 + d * sin(a), 1.2, 1.2);
    }
    cairo_fill(cr);
    break;
  case B_OIL:
    for (int k = -2; k <= 2; k++) {
      cairo_save(cr); cairo_translate(cr, 0, k * 1.4);
      cairo_set_line_width(cr, 1.0); WAVE(); cairo_stroke(cr);
      cairo_restore(cr);
    }
    break;
  case B_CRAYON:
    for (double t = 0; t <= 1.0; t += 0.03) {
      double u = 1 - t;
      double px = u*u*u*3 + 3*u*u*t*8 + 3*u*t*t*14 + t*t*t*21;
      double py = u*u*u*17 + 3*u*u*t*3 + 3*u*t*t*23 + t*t*t*7;
      for (int i = 0; i < 4; i++)
        cairo_rectangle(cr, px + g_rand_double_range(r, -2, 2), py + g_rand_double_range(r, -2, 2), 1.3, 1.3);
    }
    cairo_fill(cr);
    break;
  case B_MARKER:
    cairo_set_source_rgba(cr, fg->red, fg->green, fg->blue, 0.55);
    cairo_set_line_width(cr, 5); cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE); WAVE(); cairo_stroke(cr);
    break;
  case B_NATURAL:
    for (int k = 0; k < 3; k++) {
      cairo_save(cr); cairo_translate(cr, g_rand_double_range(r, -0.8, 0.8), g_rand_double_range(r, -0.8, 0.8));
      cairo_set_source_rgba(cr, fg->red, fg->green, fg->blue, 0.55);
      cairo_set_line_width(cr, 0.9); WAVE(); cairo_stroke(cr);
      cairo_restore(cr);
    }
    break;
  case B_WATERCOLOR: {
    cairo_pattern_t *pat = cairo_pattern_create_radial(12, 12, 1, 12, 12, 10);
    cairo_pattern_add_color_stop_rgba(pat, 0, fg->red, fg->green, fg->blue, 0.75);
    cairo_pattern_add_color_stop_rgba(pat, 1, fg->red, fg->green, fg->blue, 0);
    cairo_set_source(cr, pat);
    cairo_move_to(cr, 12, 2);
    cairo_curve_to(cr, 20, 12, 20, 21, 12, 21);
    cairo_curve_to(cr, 4, 21, 4, 12, 12, 2);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);
    break;
  }
  }
  #undef WAVE
  g_rand_free(r);
  cairo_restore(cr);
}

/* (0,0)-(24,24) maydonga chizadi; chaqiruvchi o'lchamni moslaydi */
static void draw_icon(cairo_t *cr, int id, const GdkRGBA *fg) {
  gdk_cairo_set_source_rgba(cr, fg);
  cairo_set_line_width(cr, 1.8);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  if (id >= IC_SHAPE0) {
    int s = id - IC_SHAPE0;
    cairo_set_line_width(cr, 1.6);
    if (s == SH_LINE) shape_path_box(cr, s, 4, 20, 16, -16, NULL, 0);
    else if (s == SH_CURVE) shape_path_box(cr, s, 3, 5, 18, 14, NULL, 0);
    else shape_path_box(cr, s, 3.5, 3.5, 17, 17, NULL, 0);
    cairo_stroke(cr);
    return;
  }
  if (id >= IC_BRUSH0) {
    icon_brush_sample(cr, id - IC_BRUSH0, fg);
    return;
  }
  double dash[] = { 2.5, 2 };
  switch (id) {
  case IC_SELECT:
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    cairo_set_dash(cr, dash, 2, 0);
    cairo_rectangle(cr, 3.5, 5, 17, 14);
    cairo_stroke(cr);
    break;
  case IC_FREESELECT:
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    cairo_set_dash(cr, dash, 2, 0);
    cairo_move_to(cr, 5, 8);
    cairo_curve_to(cr, 9, 1, 22, 4, 20, 11);
    cairo_curve_to(cr, 18, 19, 8, 21, 4, 16);
    cairo_curve_to(cr, 2, 13, 3, 11, 5, 8);
    cairo_stroke(cr);
    break;
  case IC_SELMENU:
    cairo_move_to(cr, 7, 10); cairo_line_to(cr, 12, 15); cairo_line_to(cr, 17, 10);
    cairo_stroke(cr);
    break;
  case IC_PENCIL:
    cairo_move_to(cr, 6, 18); cairo_line_to(cr, 17, 7); cairo_line_to(cr, 20, 10);
    cairo_line_to(cr, 9, 21); cairo_close_path(cr); cairo_stroke(cr);
    cairo_move_to(cr, 6, 18); cairo_line_to(cr, 4, 23); cairo_line_to(cr, 9, 21); cairo_fill(cr);
    break;
  case IC_FILL:
    cairo_save(cr); cairo_translate(cr, 10, 13); cairo_rotate(cr, G_PI / 4);
    cairo_rectangle(cr, -5.5, -5.5, 11, 11); cairo_restore(cr); cairo_stroke(cr);
    cairo_move_to(cr, 20, 12);
    cairo_curve_to(cr, 23.5, 17, 22.5, 20.5, 20, 20.5);
    cairo_curve_to(cr, 17.5, 20.5, 16.5, 17, 20, 12); cairo_fill(cr);
    break;
  case IC_TEXT: {
    PangoLayout *l = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_from_string("Serif Bold");
    pango_font_description_set_absolute_size(fd, 20 * PANGO_SCALE);
    pango_layout_set_font_description(l, fd);
    pango_font_description_free(fd);
    pango_layout_set_text(l, "A", -1);
    int lw, lh;
    pango_layout_get_pixel_size(l, &lw, &lh);
    cairo_move_to(cr, 12 - lw / 2.0, 12 - lh / 2.0);
    pango_cairo_show_layout(cr, l);
    g_object_unref(l);
    break;
  }
  case IC_ERASER:
    cairo_save(cr); cairo_translate(cr, 12, 12); cairo_rotate(cr, -G_PI / 4);
    cairo_rectangle(cr, -9, -4.5, 18, 9); cairo_stroke(cr);
    cairo_rectangle(cr, -9, -4.5, 7, 9); cairo_fill(cr);
    cairo_restore(cr);
    break;
  case IC_PICKER:
    cairo_set_line_width(cr, 2.4);
    cairo_move_to(cr, 4, 20); cairo_line_to(cr, 14, 10); cairo_stroke(cr);
    cairo_arc(cr, 17, 7, 4, 0, 2 * G_PI); cairo_fill(cr);
    cairo_arc(cr, 4, 20, 1.5, 0, 2 * G_PI); cairo_fill(cr);
    break;
  case IC_MAGNIFIER:
    cairo_arc(cr, 10, 10, 6, 0, 2 * G_PI); cairo_stroke(cr);
    cairo_set_line_width(cr, 3);
    cairo_move_to(cr, 14.5, 14.5); cairo_line_to(cr, 20.5, 20.5); cairo_stroke(cr);
    break;
  case IC_CROP:
    cairo_move_to(cr, 7, 2); cairo_line_to(cr, 7, 17); cairo_line_to(cr, 22, 17); cairo_stroke(cr);
    cairo_move_to(cr, 2, 7); cairo_line_to(cr, 17, 7); cairo_line_to(cr, 17, 22); cairo_stroke(cr);
    break;
  case IC_RESIZE:
    cairo_rectangle(cr, 3, 9, 12, 12); cairo_stroke(cr);
    cairo_move_to(cr, 11, 13); cairo_line_to(cr, 20, 4); cairo_stroke(cr);
    cairo_move_to(cr, 14, 4); cairo_line_to(cr, 20, 4); cairo_line_to(cr, 20, 10); cairo_stroke(cr);
    break;
  case IC_UNDO: case IC_REDO:
    if (id == IC_REDO) { cairo_translate(cr, 24, 0); cairo_scale(cr, -1, 1); }
    cairo_move_to(cr, 5, 10);
    cairo_line_to(cr, 15, 10);
    cairo_curve_to(cr, 23, 10, 23, 20, 15, 20);
    cairo_line_to(cr, 9, 20);
    cairo_stroke(cr);
    cairo_move_to(cr, 9, 6); cairo_line_to(cr, 5, 10); cairo_line_to(cr, 9, 14);
    cairo_stroke(cr);
    break;
  case IC_ROTATE:
    cairo_arc(cr, 12, 13, 7, -G_PI * 0.85, G_PI * 0.6); cairo_stroke(cr);
    cairo_move_to(cr, 3, 4); cairo_line_to(cr, 6.2, 9.8); cairo_line_to(cr, 11, 6); cairo_stroke(cr);
    break;
  }
}

/* dastur belgisi (ilova ikonasi) */
static void draw_app_icon(cairo_t *cr, double size) {
  cairo_scale(cr, size / 64.0, size / 64.0);
  rounded_rect(cr, 3, 3, 58, 58, 13);
  cairo_pattern_t *g = cairo_pattern_create_linear(0, 3, 0, 61);
  cairo_pattern_add_color_stop_rgb(g, 0, 0.23, 0.51, 0.96);
  cairo_pattern_add_color_stop_rgb(g, 1, 0.49, 0.23, 0.93);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  cairo_save(cr);
  cairo_translate(cr, 29, 35);
  cairo_scale(cr, 1, 0.82);
  cairo_arc(cr, 0, 0, 21, 0, 2 * G_PI);
  cairo_restore(cr);
  cairo_set_source_rgb(cr, 0.99, 0.97, 0.92);
  cairo_fill(cr);
  cairo_arc(cr, 22, 45, 4.5, 0, 2 * G_PI);
  cairo_set_source_rgb(cr, 0.36, 0.33, 0.85);
  cairo_fill(cr);
  const double dots[4][5] = { { 16, 32, 0.93, 0.11, 0.14 }, { 25, 24, 1.0, 0.8, 0.05 },
                              { 36, 26, 0.13, 0.69, 0.30 }, { 41, 36, 0.0, 0.55, 0.9 } };
  for (int i = 0; i < 4; i++) {
    cairo_arc(cr, dots[i][0], dots[i][1], 4.2, 0, 2 * G_PI);
    cairo_set_source_rgb(cr, dots[i][2], dots[i][3], dots[i][4]);
    cairo_fill(cr);
  }
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_width(cr, 4.5);
  cairo_set_source_rgb(cr, 0.55, 0.33, 0.16);
  cairo_move_to(cr, 56, 9); cairo_line_to(cr, 42, 27); cairo_stroke(cr);
  cairo_set_line_width(cr, 5.5);
  cairo_set_source_rgb(cr, 0.2, 0.2, 0.22);
  cairo_move_to(cr, 42, 27); cairo_line_to(cr, 38.5, 31.5); cairo_stroke(cr);
}

static GdkPixbuf *make_app_icon(int size) {
  cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
  cairo_t *cr = cairo_create(s);
  draw_app_icon(cr, size);
  cairo_destroy(cr);
  GdkPixbuf *pb = gdk_pixbuf_get_from_surface(s, 0, 0, size, size);
  cairo_surface_destroy(s);
  return pb;
}
