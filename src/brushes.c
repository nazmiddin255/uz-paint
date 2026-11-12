/*
 * Uz Paint — qalam, o'chirg'ich va 9 xil mo'yqalam.
 * Copyright (c) 2026 Uz Paint mualliflari — MIT litsenziyasi.
 */

static const GdkRGBA *stroke_color(void) { return S.btn == 3 ? &S.c2 : &S.c1; }

static void set_rgba_a(cairo_t *cr, const GdkRGBA *c, double a) {
  cairo_set_source_rgba(cr, c->red, c->green, c->blue, a);
}

/* segment bo'ylab teng oraliqda nuqtalar (stamplar) uchun */
#define FOR_STAMPS(x0, y0, x1, y1, spacing, px, py)                               \
  for (int _n = MAX(1, (int)ceil(hypot((x1) - (x0), (y1) - (y0)) / (spacing))),   \
           _i = (hypot((x1) - (x0), (y1) - (y0)) < 0.01 ? _n : 1);                 \
       _i <= _n; _i++)                                                             \
    for (double px = (x0) + ((x1) - (x0)) * _i / _n, py = (y0) + ((y1) - (y0)) * _i / _n, \
                _once = 1; _once > 0; _once = 0)

static void spray_dots(cairo_t *cr, double x, double y, double size) {
  double radius = size * 1.2 + 2;
  int n = (int)(size * 1.5) + 4;
  for (int i = 0; i < n; i++) {
    double a = g_random_double() * 2 * G_PI, d = sqrt(g_random_double()) * radius;
    cairo_rectangle(cr, floor(x + d * cos(a)), floor(y + d * sin(a)), 1, 1);
  }
  cairo_fill(cr);
}

static void brush_seg(cairo_t *cr, double x0, double y0, double x1, double y1, double sz, const GdkRGBA *c) {
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  switch (S.brush) {
  case B_NORMAL:
    gdk_cairo_set_source_rgba(cr, c);
    cairo_set_line_width(cr, sz);
    cairo_move_to(cr, x0, y0); cairo_line_to(cr, x1, y1); cairo_stroke(cr);
    break;
  case B_CALLIGRAPHY: {
    double vx = sz * 0.5 * 0.7071, vy = -sz * 0.5 * 0.7071;
    gdk_cairo_set_source_rgba(cr, c);
    cairo_move_to(cr, x0 + vx, y0 + vy); cairo_line_to(cr, x1 + vx, y1 + vy);
    cairo_line_to(cr, x1 - vx, y1 - vy); cairo_line_to(cr, x0 - vx, y0 - vy);
    cairo_close_path(cr);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    break;
  }
  case B_PEN: {
    double d = hypot(x1 - x0, y1 - y0);
    double target = sz * CLAMP(1.35 - d / (sz * 3 + 12), 0.3, 1.35);
    double w0 = S.pen_w;
    S.pen_w = S.pen_w * 0.6 + target * 0.4;
    gdk_cairo_set_source_rgba(cr, c);
    cairo_set_line_width(cr, (w0 + S.pen_w) / 2);
    cairo_move_to(cr, x0, y0); cairo_line_to(cr, x1, y1); cairo_stroke(cr);
    break;
  }
  case B_AIRBRUSH:
    gdk_cairo_set_source_rgba(cr, c);
    FOR_STAMPS(x0, y0, x1, y1, MAX(2, sz / 2), px, py) spray_dots(cr, px, py, sz);
    break;
  case B_OIL:
    for (int i = 0; i < S.nbristle; i++) {
      double *b = S.bristle[i];
      GdkRGBA cc = *c;
      cc.red = CLAMP(cc.red * b[2], 0, 1); cc.green = CLAMP(cc.green * b[2], 0, 1); cc.blue = CLAMP(cc.blue * b[2], 0, 1);
      set_rgba_a(cr, &cc, b[3]);
      cairo_set_line_width(cr, MAX(1.2, sz / (S.nbristle * 0.45)));
      cairo_move_to(cr, x0 + b[0], y0 + b[1]); cairo_line_to(cr, x1 + b[0], y1 + b[1]); cairo_stroke(cr);
    }
    break;
  case B_CRAYON:
    set_rgba_a(cr, c, 0.85);
    FOR_STAMPS(x0, y0, x1, y1, MAX(1, sz / 4), px, py) {
      int n = (int)(sz * sz * 0.18) + 3;
      for (int i = 0; i < n; i++) {
        double a = g_random_double() * 2 * G_PI, d = sqrt(g_random_double()) * sz / 2;
        cairo_rectangle(cr, px + d * cos(a), py + d * sin(a), 1.1, 1.1);
      }
      cairo_fill(cr);
    }
    break;
  case B_MARKER:
    gdk_cairo_set_source_rgba(cr, c);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    cairo_set_line_width(cr, sz * 1.5);
    cairo_move_to(cr, x0, y0); cairo_line_to(cr, x1, y1); cairo_stroke(cr);
    break;
  case B_NATURAL:
    for (int k = 0; k < 3; k++) {
      double jx = g_random_double_range(-sz * 0.3, sz * 0.3), jy = g_random_double_range(-sz * 0.3, sz * 0.3);
      set_rgba_a(cr, c, 0.4);
      cairo_set_line_width(cr, MAX(0.8, sz * 0.35));
      cairo_move_to(cr, x0 + jx, y0 + jy); cairo_line_to(cr, x1 + jx, y1 + jy); cairo_stroke(cr);
    }
    set_rgba_a(cr, c, 0.6);
    FOR_STAMPS(x0, y0, x1, y1, 2, px, py) {
      cairo_rectangle(cr, px + g_random_double_range(-sz / 2, sz / 2), py + g_random_double_range(-sz / 2, sz / 2), 1, 1);
    }
    cairo_fill(cr);
    break;
  case B_WATERCOLOR: {
    double r = sz * 0.9 + 2;
    FOR_STAMPS(x0, y0, x1, y1, MAX(1, r / 3), px, py) {
      cairo_pattern_t *pat = cairo_pattern_create_radial(px, py, 0, px, py, r);
      cairo_pattern_add_color_stop_rgba(pat, 0, c->red, c->green, c->blue, 0.5);
      cairo_pattern_add_color_stop_rgba(pat, 0.65, c->red, c->green, c->blue, 0.3);
      cairo_pattern_add_color_stop_rgba(pat, 1, c->red, c->green, c->blue, 0);
      cairo_set_source(cr, pat);
      cairo_arc(cr, px, py, r, 0, 2 * G_PI);
      cairo_fill(cr);
      cairo_pattern_destroy(pat);
    }
    break;
  }
  }
}

static void stroke_seg(double x0, double y0, double x1, double y1) {
  cairo_t *cr = cairo_create(S.layer ? S.layer : S.surf);
  double sz = cur_size();
  if (S.tool == T_PENCIL) {
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_line_cap(cr, sz <= 1.5 ? CAIRO_LINE_CAP_SQUARE : CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    if ((int)sz % 2) { x0 = floor(x0) + 0.5; y0 = floor(y0) + 0.5; x1 = floor(x1) + 0.5; y1 = floor(y1) + 0.5; }
    gdk_cairo_set_source_rgba(cr, stroke_color());
    cairo_set_line_width(cr, sz);
    cairo_move_to(cr, x0, y0); cairo_line_to(cr, x1, y1); cairo_stroke(cr);
  } else if (S.tool == T_ERASER) {
    gdk_cairo_set_source_rgba(cr, &S.c2);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    cairo_set_line_width(cr, sz);
    cairo_move_to(cr, x0, y0); cairo_line_to(cr, x1, y1); cairo_stroke(cr);
  } else {
    brush_seg(cr, x0, y0, x1, y1, sz, stroke_color());
  }
  cairo_destroy(cr);
}

static gboolean air_tick(gpointer d) {
  if (S.drag != D_DRAW || S.tool != T_BRUSH || S.brush != B_AIRBRUSH) { S.air_timer = 0; return G_SOURCE_REMOVE; }
  cairo_t *cr = cairo_create(S.surf);
  gdk_cairo_set_source_rgba(cr, stroke_color());
  spray_dots(cr, S.lx, S.ly, cur_size());
  cairo_destroy(cr);
  gtk_widget_queue_draw(S.area);
  return G_SOURCE_CONTINUE;
}

static void stroke_begin(double x, double y) {
  double sz = cur_size();
  S.pen_w = sz;
  if (S.tool == T_BRUSH && (S.brush == B_MARKER || S.brush == B_WATERCOLOR)) {
    S.layer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, S.W, S.H);
    S.layer_alpha = S.brush == B_MARKER ? 0.5 : 0.55;
  }
  if (S.tool == T_BRUSH && S.brush == B_OIL) {
    S.nbristle = CLAMP((int)(sz / 1.5), 5, 16);
    for (int i = 0; i < S.nbristle; i++) {
      double a = g_random_double() * 2 * G_PI, d = sqrt(g_random_double()) * sz / 2;
      S.bristle[i][0] = d * cos(a);
      S.bristle[i][1] = d * sin(a);
      S.bristle[i][2] = g_random_double_range(0.82, 1.12);
      S.bristle[i][3] = g_random_double_range(0.55, 0.95);
    }
  }
  if (S.tool == T_BRUSH && S.brush == B_AIRBRUSH && !S.air_timer)
    S.air_timer = g_timeout_add(40, air_tick, NULL);
  stroke_seg(x, y, x, y);
}

static void stroke_end(void) {
  if (S.layer) {
    cairo_t *cr = cairo_create(S.surf);
    cairo_set_source_surface(cr, S.layer, 0, 0);
    cairo_paint_with_alpha(cr, S.layer_alpha);
    cairo_destroy(cr);
    cairo_surface_destroy(S.layer);
    S.layer = NULL;
  }
  if (S.air_timer) { g_source_remove(S.air_timer); S.air_timer = 0; }
}
