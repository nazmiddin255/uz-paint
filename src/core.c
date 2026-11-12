/*
 * Uz Paint — hujjat, obyektlar (shakl / matn / tanlov), sichqoncha va chizish.
 * Copyright (c) 2026 Uz Paint mualliflari — MIT litsenziyasi.
 */

/* ---------- kichik yordamchilar ---------- */

static void redraw(void) {
  if (S.area) gtk_widget_queue_draw(S.area);
  if (S.thumb && S.thumb_on) gtk_widget_queue_draw(S.thumb);
}

static int size_cat(Tool t) {
  switch (t) {
  case T_PENCIL: return 0;
  case T_BRUSH: return 1;
  case T_ERASER: return 2;
  case T_SHAPE: return 3;
  default: return -1;
  }
}

static double cur_size(void) {
  int c = size_cat(S.tool);
  if (S.fl && S.fl->kind != O_IMAGE && S.fl->kind != O_TEXT) c = 3;
  return c >= 0 ? S.sizes[c] : S.sizes[3];
}

static cairo_surface_t *surf_new_color(int w, int h, const GdkRGBA *bg) {
  cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, MAX(w, 1), MAX(h, 1));
  cairo_t *cr = cairo_create(s);
  if (bg) cairo_set_source_rgb(cr, bg->red, bg->green, bg->blue);
  else cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_paint(cr);
  cairo_destroy(cr);
  return s;
}

static cairo_surface_t *surf_new(int w, int h) { return surf_new_color(w, h, NULL); }

static cairo_surface_t *surf_copy(cairo_surface_t *src) {
  int w = cairo_image_surface_get_width(src), h = cairo_image_surface_get_height(src);
  cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
  cairo_t *cr = cairo_create(s);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_surface(cr, src, 0, 0);
  cairo_paint(cr);
  cairo_destroy(cr);
  return s;
}

static gsize surf_bytes(cairo_surface_t *s) {
  return (gsize)cairo_image_surface_get_stride(s) * cairo_image_surface_get_height(s);
}

static void msg_dialog(GtkMessageType type, const char *title, const char *msg) {
  GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(S.win), GTK_DIALOG_MODAL, type,
                                        GTK_BUTTONS_CLOSE, "%s", title);
  gtk_button_set_label(GTK_BUTTON(gtk_dialog_get_widget_for_response(GTK_DIALOG(d), GTK_RESPONSE_CLOSE)), T("ui.close"));
  if (msg) gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", msg);
  gtk_dialog_run(GTK_DIALOG(d));
  gtk_widget_destroy(d);
}

static void update_title(void) {
  char *base = S.filename ? g_path_get_basename(S.filename) : g_strdup(T("ui.untitled"));
  char *t = g_strdup_printf("%s%s — %s", S.dirty ? "*" : "", base, APP_NAME);
  gtk_window_set_title(GTK_WINDOW(S.win), t);
  g_free(t);
  g_free(base);
}

static void obj_bbox(const Obj *o, double *x0, double *y0, double *x1, double *y1);

static void update_status(void) {
  if (!S.st_pos) return;
  char b[128];
  if (S.mouse_in && S.mouse_x >= 0 && S.mouse_y >= 0 && S.mouse_x < S.W && S.mouse_y < S.H)
    g_snprintf(b, sizeof b, "%d, %d px", (int)S.mouse_x, (int)S.mouse_y);
  else b[0] = 0;
  gtk_label_set_text(GTK_LABEL(S.st_pos), b);
  if (S.fl) {
    double x0, y0, x1, y1;
    obj_bbox(S.fl, &x0, &y0, &x1, &y1);
    g_snprintf(b, sizeof b, T("st.sel"), (int)lround(x1 - x0), (int)lround(y1 - y0));
  } else b[0] = 0;
  gtk_label_set_text(GTK_LABEL(S.st_sel), b);
  g_snprintf(b, sizeof b, T("st.size"), S.W, S.H);
  gtk_label_set_text(GTK_LABEL(S.st_size), b);
  b[0] = 0;
  if (S.filename) {
    GStatBuf st;
    if (g_stat(S.filename, &st) == 0) {
      char *fs = g_format_size(st.st_size);
      g_strlcpy(b, fs, sizeof b);
      g_free(fs);
    }
  }
  gtk_label_set_text(GTK_LABEL(S.st_file), b);
}

static void update_rulers(void) {
  if (S.rulers && S.hruler) {
    gtk_widget_queue_draw(S.hruler);
    gtk_widget_queue_draw(S.vruler);
  }
}

static void update_size(void) {
  gtk_widget_set_size_request(S.area, (int)ceil(S.W * S.zoom + 2 * PAD), (int)ceil(S.H * S.zoom + 2 * PAD));
  char b[32];
  g_snprintf(b, sizeof b, "%d%%", (int)lround(S.zoom * 100));
  gtk_label_set_text(GTK_LABEL(S.zoom_label), b);
  S.updating = TRUE;
  gtk_range_set_value(GTK_RANGE(S.zoom_scale), log2(S.zoom));
  S.updating = FALSE;
  update_status();
  update_rulers();
  redraw();
}

static void set_surface(cairo_surface_t *s) {
  S.surf = s;
  S.W = cairo_image_surface_get_width(s);
  S.H = cairo_image_surface_get_height(s);
  update_size();
}

static void mark_dirty(void) {
  if (!S.dirty) { S.dirty = TRUE; update_title(); }
}

/* ---------- bekor qilish / qaytarish ---------- */

static void free_list(GList **l) {
  g_list_free_full(*l, (GDestroyNotify)cairo_surface_destroy);
  *l = NULL;
}

static void push_undo(void) {
  cairo_surface_t *c = surf_copy(S.surf);
  S.undo = g_list_prepend(S.undo, c);
  S.undo_bytes += surf_bytes(c);
  while (S.undo && (g_list_length(S.undo) > MAX_UNDO || S.undo_bytes > UNDO_BYTES) && S.undo->next) {
    GList *last = g_list_last(S.undo);
    S.undo_bytes -= surf_bytes(last->data);
    cairo_surface_destroy(last->data);
    S.undo = g_list_delete_link(S.undo, last);
  }
  free_list(&S.redo);
  mark_dirty();
}

static cairo_surface_t *pop_undo(void) {
  cairo_surface_t *s = S.undo->data;
  S.undo_bytes -= surf_bytes(s);
  S.undo = g_list_delete_link(S.undo, S.undo);
  return s;
}

static void obj_free(Obj *o);

static void do_undo(void) {
  if (S.poly_active) { S.poly_active = FALSE; g_array_set_size(S.path, 0); redraw(); return; }
  if (S.fl) {
    gboolean lifted = S.fl->lifted;
    obj_free(S.fl);
    S.fl = NULL;
    S.drag = D_NONE;
    if (lifted && S.undo) {
      cairo_surface_destroy(S.surf);
      set_surface(pop_undo());
    }
    update_status();
    redraw();
    return;
  }
  if (!S.undo) return;
  S.redo = g_list_prepend(S.redo, S.surf);
  set_surface(pop_undo());
  mark_dirty();
}

static void do_redo(void) {
  if (S.fl || !S.redo) return;
  S.undo = g_list_prepend(S.undo, S.surf);
  S.undo_bytes += surf_bytes(S.surf);
  cairo_surface_t *s = S.redo->data;
  S.redo = g_list_delete_link(S.redo, S.redo);
  set_surface(s);
  mark_dirty();
}

/* ---------- obyektlar ---------- */

static Obj *obj_new(ObjKind k) {
  Obj *o = g_new0(Obj, 1);
  o->kind = k;
  return o;
}

static void obj_free(Obj *o) {
  if (!o) return;
  g_free(o->poly);
  g_free(o->text);
  g_free(o->font);
  if (o->img) cairo_surface_destroy(o->img);
  if (o->img_key) cairo_surface_destroy(o->img_key);
  g_free(o);
}

static void obj_bbox(const Obj *o, double *x0, double *y0, double *x1, double *y1) {
  if (o->kind == O_POINTS) {
    *x0 = *x1 = o->pts[0];
    *y0 = *y1 = o->pts[1];
    for (int i = 1; i < o->npts; i++) {
      *x0 = MIN(*x0, o->pts[2 * i]); *x1 = MAX(*x1, o->pts[2 * i]);
      *y0 = MIN(*y0, o->pts[2 * i + 1]); *y1 = MAX(*y1, o->pts[2 * i + 1]);
    }
    return;
  }
  double cx = o->x + o->w / 2, cy = o->y + o->h / 2, c = cos(o->angle), s = sin(o->angle);
  *x0 = *y0 = 1e18; *x1 = *y1 = -1e18;
  for (int i = 0; i < 4; i++) {
    double dx = ((i == 1 || i == 2) ? o->w : 0) - o->w / 2, dy = (i >= 2 ? o->h : 0) - o->h / 2;
    double px = cx + dx * c - dy * s, py = cy + dx * s + dy * c;
    *x0 = MIN(*x0, px); *x1 = MAX(*x1, px); *y0 = MIN(*y0, py); *y1 = MAX(*y1, py);
  }
}

static void obj_center(const Obj *o, double *cx, double *cy) {
  if (o->kind == O_POINTS) {
    double x0, y0, x1, y1;
    obj_bbox(o, &x0, &y0, &x1, &y1);
    *cx = (x0 + x1) / 2; *cy = (y0 + y1) / 2;
    return;
  }
  *cx = o->x + o->w / 2;
  *cy = o->y + o->h / 2;
}

static PangoLayout *make_layout(cairo_t *cr, Obj *o) {
  PangoLayout *l = pango_cairo_create_layout(cr);
  PangoFontDescription *fd = pango_font_description_from_string(o->font);
  pango_layout_set_font_description(l, fd);
  pango_font_description_free(fd);
  pango_layout_set_text(l, o->text, -1);
  return l;
}

static void text_natural_size(Obj *o, double *w, double *h) {
  cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
  cairo_t *cr = cairo_create(s);
  PangoLayout *l = make_layout(cr, o);
  int lw, lh;
  pango_layout_get_pixel_size(l, &lw, &lh);
  *w = MAX(lw, 1) + 8;
  *h = MAX(lh, 1) + 4;
  g_object_unref(l);
  cairo_destroy(cr);
  cairo_surface_destroy(s);
}

/* shaffof tanlash: 2-rangga teng piksellar ko'rinmas bo'ladi */
static cairo_surface_t *obj_img(Obj *o) {
  if (!S.transparent_sel) return o->img;
  if (o->img_key && gdk_rgba_equal(&o->key_color, &S.c2)) return o->img_key;
  if (o->img_key) cairo_surface_destroy(o->img_key);
  o->img_key = surf_copy(o->img);
  o->key_color = S.c2;
  cairo_surface_flush(o->img_key);
  guint8 *d = cairo_image_surface_get_data(o->img_key);
  int st = cairo_image_surface_get_stride(o->img_key);
  int w = cairo_image_surface_get_width(o->img_key), h = cairo_image_surface_get_height(o->img_key);
  int kr = (int)lround(S.c2.red * 255), kg = (int)lround(S.c2.green * 255), kb = (int)lround(S.c2.blue * 255);
  for (int y = 0; y < h; y++) {
    guint32 *row = (guint32 *)(d + y * st);
    for (int x = 0; x < w; x++) {
      guint32 p = row[x];
      if ((p >> 24) != 255) continue;
      if (abs((int)((p >> 16) & 255) - kr) <= 10 && abs((int)((p >> 8) & 255) - kg) <= 10 &&
          abs((int)(p & 255) - kb) <= 10)
        row[x] = 0;
    }
  }
  cairo_surface_mark_dirty(o->img_key);
  return o->img_key;
}

static void render_obj(cairo_t *cr, Obj *o) {
  cairo_save(cr);
  if (o->kind == O_POINTS) {
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, o->width);
    gdk_cairo_set_source_rgba(cr, &o->stroke);
    cairo_move_to(cr, o->pts[0], o->pts[1]);
    if (o->npts == 2) cairo_line_to(cr, o->pts[2], o->pts[3]);
    else cairo_curve_to(cr, o->pts[2], o->pts[3], o->pts[4], o->pts[5], o->pts[6], o->pts[7]);
    cairo_stroke(cr);
    cairo_restore(cr);
    return;
  }
  double cx, cy;
  obj_center(o, &cx, &cy);
  cairo_translate(cr, cx, cy);
  cairo_rotate(cr, o->angle);
  cairo_translate(cr, -cx, -cy);
  switch (o->kind) {
  case O_SHAPE:
    shape_path_box(cr, o->shape, o->x, o->y, o->w, o->h, o->poly, o->npoly);
    if (o->filled) {
      gdk_cairo_set_source_rgba(cr, &o->fill);
      if (o->outline) cairo_fill_preserve(cr); else cairo_fill(cr);
    }
    if (o->outline) {
      cairo_set_line_width(cr, o->width);
      cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
      cairo_set_miter_limit(cr, 4);
      gdk_cairo_set_source_rgba(cr, &o->stroke);
      cairo_stroke(cr);
    }
    cairo_new_path(cr);
    break;
  case O_TEXT: {
    if (o->text_bg) {
      cairo_rectangle(cr, o->x, o->y, o->w, o->h);
      gdk_cairo_set_source_rgba(cr, &o->fill);
      cairo_fill(cr);
    }
    PangoLayout *l = make_layout(cr, o);
    int lw, lh;
    pango_layout_get_pixel_size(l, &lw, &lh);
    if (lw > 0 && lh > 0) {
      cairo_translate(cr, o->x, o->y);
      cairo_scale(cr, o->w / (lw + 8.0), o->h / (lh + 4.0));
      cairo_move_to(cr, 4, 2);
      gdk_cairo_set_source_rgba(cr, &o->stroke);
      pango_cairo_show_layout(cr, l);
    }
    g_object_unref(l);
    break;
  }
  case O_IMAGE: {
    cairo_surface_t *img = obj_img(o);
    int iw = cairo_image_surface_get_width(img), ih = cairo_image_surface_get_height(img);
    cairo_translate(cr, o->x, o->y);
    cairo_scale(cr, o->w / iw, o->h / ih);
    cairo_rectangle(cr, 0, 0, iw, ih);
    cairo_clip(cr);
    cairo_set_source_surface(cr, img, 0, 0);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
    gboolean exact = fabs(o->angle) < 1e-6 && fabs(o->w - iw) < 0.01 && fabs(o->h - ih) < 0.01 &&
                     fabs(o->x - round(o->x)) < 0.01 && fabs(o->y - round(o->y)) < 0.01;
    cairo_pattern_set_filter(cairo_get_source(cr), exact ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    break;
  }
  default:
    break;
  }
  cairo_restore(cr);
}

static void commit(void) {
  if (!S.fl) return;
  if (!S.fl->lifted) push_undo();
  cairo_t *cr = cairo_create(S.surf);
  render_obj(cr, S.fl);
  cairo_destroy(cr);
  obj_free(S.fl);
  S.fl = NULL;
  S.drag = D_NONE;
  mark_dirty();
  update_status();
  redraw();
}

static void delete_selection(void) {
  if (!S.fl) return;
  obj_free(S.fl);
  S.fl = NULL;
  S.drag = D_NONE;
  mark_dirty();
  update_status();
  redraw();
}

/* tanlovni obyektning to'liq chegarasiga chizadi (nusxa olish, qirqish uchun) */
static cairo_surface_t *obj_to_surface(Obj *o, int *ox, int *oy) {
  double x0, y0, x1, y1;
  obj_bbox(o, &x0, &y0, &x1, &y1);
  if (o->kind == O_POINTS || o->kind == O_SHAPE) {
    double m = o->width / 2 + 1;
    x0 -= m; y0 -= m; x1 += m; y1 += m;
  }
  int ix = (int)floor(x0), iy = (int)floor(y0);
  int w = MAX(1, (int)ceil(x1) - ix), h = MAX(1, (int)ceil(y1) - iy);
  cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
  cairo_t *cr = cairo_create(s);
  cairo_translate(cr, -ix, -iy);
  render_obj(cr, o);
  cairo_destroy(cr);
  if (ox) *ox = ix;
  if (oy) *oy = iy;
  return s;
}

/* ---------- koordinatalar va tutqichlar ---------- */

static const int HSX[8] = { -1, 1, 1, -1, 0, 1, 0, -1 };
static const int HSY[8] = { -1, -1, 1, 1, -1, 0, 1, 0 };

static void rot_point(const Obj *o, double ang, double px, double py, double *rx, double *ry) {
  double cx, cy;
  obj_center(o, &cx, &cy);
  double dx = px - cx, dy = py - cy, c = cos(ang), s = sin(ang);
  *rx = cx + dx * c - dy * s;
  *ry = cy + dx * s + dy * c;
}
static void to_local(const Obj *o, double px, double py, double *lx, double *ly) { rot_point(o, -o->angle, px, py, lx, ly); }
static void to_world(const Obj *o, double lx, double ly, double *px, double *py) { rot_point(o, o->angle, lx, ly, px, py); }

static void rot_handle_local(const Obj *o, double *hx, double *hy) {
  double off = ROT_OFF / S.zoom;
  *hx = o->x + o->w / 2;
  *hy = o->y - off;
  if (fabs(o->angle) < 1e-6 && o->y - off < 0) *hy = o->y + o->h + off;
}

static void handle_local(const Obj *o, int i, double *hx, double *hy) {
  *hx = o->x + (HSX[i] + 1) / 2.0 * o->w;
  *hy = o->y + (HSY[i] + 1) / 2.0 * o->h;
}

/* qaytaradi: -1 hech narsa; 0..7 o'lcham tutqichlari; 8 burish;
 * O_POINTS uchun 0..npts-1 nuqtalar */
static int hit_handle(const Obj *o, double px, double py) {
  double r = HANDLE_R / S.zoom;
  if (o->kind == O_POINTS) {
    for (int i = o->npts - 1; i >= 0; i--)
      if (hypot(px - o->pts[2 * i], py - o->pts[2 * i + 1]) <= r) return i;
    return -1;
  }
  double lx, ly, hx, hy;
  to_local(o, px, py, &lx, &ly);
  rot_handle_local(o, &hx, &hy);
  if (hypot(lx - hx, ly - hy) <= r) return 8;
  for (int i = 0; i < 8; i++) {
    if (i >= 4 && (o->w * S.zoom < 30 && HSY[i] == 0)) continue;
    if (i >= 4 && (o->h * S.zoom < 30 && HSX[i] == 0)) continue;
    handle_local(o, i, &hx, &hy);
    if (fabs(lx - hx) <= r && fabs(ly - hy) <= r) return i;
  }
  return -1;
}

static double seg_dist(double px, double py, double x1, double y1, double x2, double y2) {
  double dx = x2 - x1, dy = y2 - y1, l2 = dx * dx + dy * dy;
  double t = l2 > 0 ? ((px - x1) * dx + (py - y1) * dy) / l2 : 0;
  t = CLAMP(t, 0, 1);
  return hypot(px - (x1 + t * dx), py - (y1 + t * dy));
}

static gboolean hit_body(const Obj *o, double px, double py) {
  double tol = MAX(o->width / 2, 6 / S.zoom);
  if (o->kind == O_POINTS) {
    if (o->npts == 2) return seg_dist(px, py, o->pts[0], o->pts[1], o->pts[2], o->pts[3]) <= tol;
    double lx = o->pts[0], ly = o->pts[1];
    for (int i = 1; i <= 40; i++) {
      double t = i / 40.0, u = 1 - t;
      double x = u*u*u*o->pts[0] + 3*u*u*t*o->pts[2] + 3*u*t*t*o->pts[4] + t*t*t*o->pts[6];
      double y = u*u*u*o->pts[1] + 3*u*u*t*o->pts[3] + 3*u*t*t*o->pts[5] + t*t*t*o->pts[7];
      if (seg_dist(px, py, lx, ly, x, y) <= tol) return TRUE;
      lx = x; ly = y;
    }
    return FALSE;
  }
  double lx, ly, m = 4 / S.zoom;
  to_local(o, px, py, &lx, &ly);
  return lx >= o->x - m && lx <= o->x + o->w + m && ly >= o->y - m && ly <= o->y + o->h + m;
}

static void handle_square(cairo_t *cr, double x, double y, double hs) {
  cairo_rectangle(cr, x - hs, y - hs, 2 * hs, 2 * hs);
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, 0.15, 0.4, 0.95);
  cairo_stroke(cr);
}

static void handle_dot(cairo_t *cr, double x, double y, double r) {
  cairo_arc(cr, x, y, r, 0, 2 * G_PI);
  cairo_set_source_rgb(cr, 0.3, 0.85, 0.4);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, 0.1, 0.35, 0.15);
  cairo_stroke(cr);
}

static void draw_handles(cairo_t *cr, Obj *o) {
  double z = S.zoom, hs = 4.5 / z, d = 4 / z;
  cairo_save(cr);
  cairo_set_line_width(cr, 1.2 / z);
  if (o->kind == O_POINTS) {
    if (o->npts == 4) {
      cairo_set_dash(cr, &d, 1, 0);
      cairo_set_source_rgba(cr, 0.15, 0.4, 0.95, 0.8);
      cairo_move_to(cr, o->pts[0], o->pts[1]); cairo_line_to(cr, o->pts[2], o->pts[3]);
      cairo_move_to(cr, o->pts[6], o->pts[7]); cairo_line_to(cr, o->pts[4], o->pts[5]);
      cairo_stroke(cr);
      cairo_set_dash(cr, NULL, 0, 0);
      handle_dot(cr, o->pts[2], o->pts[3], 5.5 / z);
      handle_dot(cr, o->pts[4], o->pts[5], 5.5 / z);
      handle_square(cr, o->pts[0], o->pts[1], hs);
      handle_square(cr, o->pts[6], o->pts[7], hs);
    } else {
      handle_square(cr, o->pts[0], o->pts[1], hs);
      handle_square(cr, o->pts[2], o->pts[3], hs);
    }
    cairo_restore(cr);
    return;
  }
  double cx, cy, hx, hy;
  obj_center(o, &cx, &cy);
  cairo_translate(cr, cx, cy);
  cairo_rotate(cr, o->angle);
  cairo_translate(cr, -cx, -cy);
  cairo_rectangle(cr, o->x, o->y, o->w, o->h);
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_stroke_preserve(cr);
  cairo_set_dash(cr, &d, 1, 0);
  cairo_set_source_rgb(cr, 0.15, 0.4, 0.95);
  cairo_stroke(cr);
  cairo_set_dash(cr, NULL, 0, 0);
  rot_handle_local(o, &hx, &hy);
  cairo_move_to(cr, cx, hy < o->y ? o->y : o->y + o->h);
  cairo_line_to(cr, hx, hy);
  cairo_set_source_rgb(cr, 0.15, 0.4, 0.95);
  cairo_stroke(cr);
  handle_dot(cr, hx, hy, 5.5 / z);
  for (int i = 0; i < 8; i++) {
    if (i >= 4 && (o->w * z < 30 && HSY[i] == 0)) continue;
    if (i >= 4 && (o->h * z < 30 && HSX[i] == 0)) continue;
    handle_local(o, i, &hx, &hy);
    handle_square(cr, hx, hy, hs);
  }
  cairo_restore(cr);
}

/* ---------- rasmdan bo'lak kesib olish ---------- */

static void make_lifted(cairo_surface_t *img, int x0, int y0) {
  Obj *o = obj_new(O_IMAGE);
  o->x = x0; o->y = y0;
  o->w = cairo_image_surface_get_width(img);
  o->h = cairo_image_surface_get_height(img);
  o->img = img;
  o->lifted = TRUE;
  S.fl = o;
  update_status();
  redraw();
}

static void lift_rect(int x0, int y0, int x1, int y1) {
  x0 = CLAMP(x0, 0, S.W); x1 = CLAMP(x1, 0, S.W);
  y0 = CLAMP(y0, 0, S.H); y1 = CLAMP(y1, 0, S.H);
  int w = x1 - x0, h = y1 - y0;
  if (w < 1 || h < 1) return;
  push_undo();
  cairo_surface_t *img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
  cairo_t *cr = cairo_create(img);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_surface(cr, S.surf, -x0, -y0);
  cairo_paint(cr);
  cairo_destroy(cr);
  cr = cairo_create(S.surf);
  cairo_rectangle(cr, x0, y0, w, h);
  gdk_cairo_set_source_rgba(cr, &S.c2);
  cairo_fill(cr);
  cairo_destroy(cr);
  make_lifted(img, x0, y0);
}

static void path_to_cairo(cairo_t *cr, GArray *p, double dx, double dy) {
  double *v = (double *)p->data;
  int n = p->len / 2;
  cairo_move_to(cr, v[0] + dx, v[1] + dy);
  for (int i = 1; i < n; i++) cairo_line_to(cr, v[2 * i] + dx, v[2 * i + 1] + dy);
  cairo_close_path(cr);
}

static void lift_lasso(void) {
  int n = S.path->len / 2;
  if (n < 3) return;
  double *v = (double *)S.path->data, minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
  for (int i = 0; i < n; i++) {
    minx = MIN(minx, v[2 * i]); maxx = MAX(maxx, v[2 * i]);
    miny = MIN(miny, v[2 * i + 1]); maxy = MAX(maxy, v[2 * i + 1]);
  }
  int x0 = CLAMP((int)floor(minx), 0, S.W), y0 = CLAMP((int)floor(miny), 0, S.H);
  int x1 = CLAMP((int)ceil(maxx), 0, S.W), y1 = CLAMP((int)ceil(maxy), 0, S.H);
  int w = x1 - x0, h = y1 - y0;
  if (w < 2 || h < 2) return;
  push_undo();
  cairo_surface_t *img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
  cairo_t *cr = cairo_create(img);
  path_to_cairo(cr, S.path, -x0, -y0);
  cairo_clip(cr);
  cairo_set_source_surface(cr, S.surf, -x0, -y0);
  cairo_paint(cr);
  cairo_destroy(cr);
  cr = cairo_create(S.surf);
  path_to_cairo(cr, S.path, 0, 0);
  gdk_cairo_set_source_rgba(cr, &S.c2);
  cairo_fill(cr);
  cairo_destroy(cr);
  make_lifted(img, x0, y0);
}

static void select_tool(Tool t, int sub);

static void select_all(void) {
  commit();
  if (S.tool != T_SELECT && S.tool != T_FREESELECT) select_tool(T_SELECT, 0);
  lift_rect(0, 0, S.W, S.H);
}

/* ---------- mo'yqalamlar ---------- */
#include "brushes.c"

/* ---------- bo'yash va rang olish ---------- */

static gboolean px_near(guint32 a, guint32 b, int tol) {
  for (int sh = 0; sh <= 24; sh += 8)
    if (abs((int)((a >> sh) & 255) - (int)((b >> sh) & 255)) > tol) return FALSE;
  return TRUE;
}

static void flood_fill(int x, int y, const GdkRGBA *c) {
  if (x < 0 || y < 0 || x >= S.W || y >= S.H) return;
  cairo_surface_flush(S.surf);
  guint8 *data = cairo_image_surface_get_data(S.surf);
  int stride = cairo_image_surface_get_stride(S.surf), W = S.W, H = S.H;
  guint32 nc = 0xFF000000u | ((guint32)lround(c->red * 255) << 16) |
               ((guint32)lround(c->green * 255) << 8) | (guint32)lround(c->blue * 255);
  guint32 target = ((guint32 *)(data + y * stride))[x];
  if (target == nc) return;
  push_undo();
  data = cairo_image_surface_get_data(S.surf);
  guint8 *seen = g_malloc0((gsize)W * H);
  gint *stack = g_malloc(sizeof(gint) * (gsize)W * H);
  gsize sp = 0;
  stack[sp++] = y * W + x;
  seen[y * W + x] = 1;
  while (sp) {
    int i = stack[--sp], px = i % W, py = i / W;
    ((guint32 *)(data + py * stride))[px] = nc;
    const int nb[4][2] = { { px - 1, py }, { px + 1, py }, { px, py - 1 }, { px, py + 1 } };
    for (int k = 0; k < 4; k++) {
      int nx = nb[k][0], ny = nb[k][1];
      if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
      int j = ny * W + nx;
      if (seen[j]) continue;
      if (!px_near(((guint32 *)(data + ny * stride))[nx], target, 32)) continue;
      seen[j] = 1;
      stack[sp++] = j;
    }
  }
  g_free(seen);
  g_free(stack);
  cairo_surface_mark_dirty(S.surf);
}

static void set_color(int which, const GdkRGBA *c);

static void pick_color(int x, int y, int button) {
  if (x < 0 || y < 0 || x >= S.W || y >= S.H) return;
  cairo_surface_flush(S.surf);
  guint8 *data = cairo_image_surface_get_data(S.surf);
  guint32 p = ((guint32 *)(data + y * cairo_image_surface_get_stride(S.surf)))[x];
  GdkRGBA c = { ((p >> 16) & 255) / 255.0, ((p >> 8) & 255) / 255.0, (p & 255) / 255.0, 1 };
  set_color(button == 3 ? 2 : 1, &c);
  select_tool(S.prev_tool, S.prev_sub);
}

/* ---------- matn ---------- */

static gboolean on_text_key(GtkWidget *w, GdkEventKey *e, gpointer dlg) {
  if ((e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) && !(e->state & GDK_SHIFT_MASK)) {
    gtk_dialog_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    return TRUE;
  }
  return FALSE;
}

static void save_settings(void);

static void text_dialog(Obj *edit, double x, double y, const GdkRGBA *col) {
  GtkWidget *dlg = gtk_dialog_new_with_buttons(edit ? T("text.edit") : T("text.new"),
      GTK_WINDOW(S.win), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      T("ui.cancel"), GTK_RESPONSE_CANCEL, edit ? T("ui.ok") : T("text.add"), GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
  GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
  gtk_container_set_border_width(GTK_CONTAINER(box), 12);
  gtk_box_set_spacing(GTK_BOX(box), 8);
  GtkWidget *lab = gtk_label_new(T("text.prompt"));
  gtk_widget_set_halign(lab, GTK_ALIGN_START);
  gtk_box_pack_start(GTK_BOX(box), lab, FALSE, FALSE, 0);
  GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
  gtk_widget_set_size_request(sw, 440, 150);
  GtkWidget *tv = gtk_text_view_new();
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD_CHAR);
  if (edit) gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv)), edit->text, -1);
  g_signal_connect(tv, "key-press-event", G_CALLBACK(on_text_key), dlg);
  gtk_container_add(GTK_CONTAINER(sw), tv);
  gtk_box_pack_start(GTK_BOX(box), sw, TRUE, TRUE, 0);
  GtkWidget *hint = gtk_label_new(T("text.hint"));
  gtk_widget_set_halign(hint, GTK_ALIGN_START);
  gtk_style_context_add_class(gtk_widget_get_style_context(hint), "dim-label");
  gtk_box_pack_start(GTK_BOX(box), hint, FALSE, FALSE, 0);
  GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start(GTK_BOX(hb), gtk_label_new(T("text.font")), FALSE, FALSE, 0);
  GtkWidget *fb = gtk_font_button_new_with_font(edit ? edit->font : S.text_font);
  gtk_box_pack_start(GTK_BOX(hb), fb, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), hb, FALSE, FALSE, 0);
  GtkWidget *bg = gtk_check_button_new_with_label(T("text.bg"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bg), edit ? edit->text_bg : FALSE);
  gtk_box_pack_start(GTK_BOX(box), bg, FALSE, FALSE, 0);
  gtk_widget_show_all(dlg);
  gtk_widget_grab_focus(tv);

  if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
    GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(b, &s, &e);
    char *txt = gtk_text_buffer_get_text(b, &s, &e, FALSE);
    if (txt) g_strchomp(txt);
    char *font = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(fb));
    if (font) { g_free(S.text_font); S.text_font = g_strdup(font); save_settings(); }
    if (txt && *txt) {
      Obj *o = edit;
      if (!o) {
        o = obj_new(O_TEXT);
        o->x = x; o->y = y;
        o->stroke = *col;
        o->fill = S.c2;
        S.fl = o;
      }
      g_free(o->text); o->text = g_strdup(txt);
      g_free(o->font); o->font = g_strdup(S.text_font);
      o->text_bg = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bg));
      text_natural_size(o, &o->w, &o->h);
      mark_dirty();
    } else if (edit) {
      delete_selection();
    }
    g_free(txt);
    g_free(font);
  }
  gtk_widget_destroy(dlg);
  update_status();
  redraw();
}

/* ---------- ko'pburchak ---------- */

static gboolean cur_outline(void);
static gboolean cur_filled(void);

static void finish_polygon(void) {
  if (!S.poly_active) return;
  S.poly_active = FALSE;
  double *v = (double *)S.path->data;
  int n = S.path->len / 2;
  /* ketma-ket takrorlangan nuqtalarni olib tashlaymiz */
  int m = 0;
  for (int i = 0; i < n; i++) {
    if (m > 0 && hypot(v[2 * i] - v[2 * (m - 1)], v[2 * i + 1] - v[2 * (m - 1) + 1]) < 1.5) continue;
    v[2 * m] = v[2 * i]; v[2 * m + 1] = v[2 * i + 1]; m++;
  }
  if (m >= 3) {
    double minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
    for (int i = 0; i < m; i++) {
      minx = MIN(minx, v[2 * i]); maxx = MAX(maxx, v[2 * i]);
      miny = MIN(miny, v[2 * i + 1]); maxy = MAX(maxy, v[2 * i + 1]);
    }
    double w = MAX(maxx - minx, 1), h = MAX(maxy - miny, 1);
    Obj *o = obj_new(O_SHAPE);
    o->shape = SH_POLYGON;
    o->x = minx; o->y = miny; o->w = w; o->h = h;
    o->npoly = m;
    o->poly = g_new(double, 2 * m);
    for (int i = 0; i < m; i++) {
      o->poly[2 * i] = (v[2 * i] - minx) / w;
      o->poly[2 * i + 1] = (v[2 * i + 1] - miny) / h;
    }
    o->stroke = S.c1; o->fill = S.c2;
    o->width = S.sizes[3];
    o->outline = cur_outline(); o->filled = cur_filled();
    if (!o->outline && !o->filled) o->outline = TRUE;
    S.fl = o;
    mark_dirty();
  }
  g_array_set_size(S.path, 0);
  update_status();
  redraw();
}

/* ---------- kattalashtirish ---------- */

static GtkAdjustment *hadj(void) { return gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(S.scroll)); }
static GtkAdjustment *vadj(void) { return gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(S.scroll)); }

/* (vx, vy) — ko'rinadigan maydondagi nuqta; manfiy bo'lsa markaz */
static void set_zoom(double z, double vx, double vy) {
  z = CLAMP(z, 0.125, 8);
  if (fabs(z - S.zoom) < 1e-9) { update_size(); return; }
  GtkAdjustment *ha = hadj(), *va = vadj();
  if (vx < 0) vx = gtk_adjustment_get_page_size(ha) / 2;
  if (vy < 0) vy = gtk_adjustment_get_page_size(va) / 2;
  S.anchor_ix = (gtk_adjustment_get_value(ha) + vx - PAD) / S.zoom;
  S.anchor_iy = (gtk_adjustment_get_value(va) + vy - PAD) / S.zoom;
  S.anchor_vx = vx;
  S.anchor_vy = vy;
  S.pending_anchor = TRUE;
  S.zoom = z;
  update_size();
}

static const double ZOOMS[] = { 0.125, 0.25, 0.333333, 0.5, 0.666667, 1, 1.5, 2, 3, 4, 5, 6, 8 };

static void zoom_step(int dir, double vx, double vy) {
  int n = G_N_ELEMENTS(ZOOMS);
  double z = S.zoom;
  if (dir > 0) { for (int i = 0; i < n; i++) if (ZOOMS[i] > z + 1e-6) { z = ZOOMS[i]; break; } }
  else { for (int i = n - 1; i >= 0; i--) if (ZOOMS[i] < z - 1e-6) { z = ZOOMS[i]; break; } }
  set_zoom(z, vx, vy);
}

static void zoom_fit(void) {
  GtkAllocation a;
  gtk_widget_get_allocation(S.scroll, &a);
  double z = MIN((a.width - 2 * PAD - 4) / (double)S.W, (a.height - 2 * PAD - 4) / (double)S.H);
  set_zoom(CLAMP(z, 0.125, 8), -1, -1);
}

static void on_area_allocate(GtkWidget *w, GdkRectangle *a, gpointer d) {
  if (!S.pending_anchor) return;
  S.pending_anchor = FALSE;
  gtk_adjustment_set_value(hadj(), PAD + S.anchor_ix * S.zoom - S.anchor_vx);
  gtk_adjustment_set_value(vadj(), PAD + S.anchor_iy * S.zoom - S.anchor_vy);
}

/* ---------- sichqoncha ---------- */

static int hit_canvas_handle(double ex, double ey) {
  double r = 7, cw = PAD + S.W * S.zoom, ch = PAD + S.H * S.zoom;
  if (fabs(ex - (cw + 4)) <= r && fabs(ey - (ch + 4)) <= r) return 3;
  if (fabs(ex - (cw + 4)) <= r && fabs(ey - (PAD + S.H * S.zoom / 2)) <= r) return 1;
  if (fabs(ex - (PAD + S.W * S.zoom / 2)) <= r && fabs(ey - (ch + 4)) <= r) return 2;
  return 0;
}

static void set_cursor_name(const char *name) {
  static const char *last = NULL;
  if (name == last) return;
  GdkWindow *gw = gtk_widget_get_window(S.area);
  if (!gw) return;
  last = name;
  GdkCursor *c = gdk_cursor_new_from_name(gdk_window_get_display(gw), name);
  gdk_window_set_cursor(gw, c);
  if (c) g_object_unref(c);
}

static void update_cursor(double x, double y, double ex, double ey) {
  const char *name = "crosshair";
  if (S.tool == T_TEXT) name = "text";
  else if (S.tool == T_MAGNIFIER) name = "zoom-in";
  int ch = hit_canvas_handle(ex, ey);
  if (ch == 1) name = "ew-resize";
  else if (ch == 2) name = "ns-resize";
  else if (ch == 3) name = "nwse-resize";
  if (S.fl && S.drag == D_NONE) {
    int h = hit_handle(S.fl, x, y);
    if (S.fl->kind == O_POINTS && h >= 0) name = "pointer";
    else if (h == 8) name = "grab";
    else if (h >= 0) {
      /* burilgan holatda ham to'g'ri kursor */
      double ang = atan2(HSY[h], HSX[h]) + S.fl->angle;
      int oct = ((int)lround(ang / (G_PI / 4)) % 4 + 4) % 4;
      static const char *cur[4] = { "ew-resize", "nwse-resize", "ns-resize", "nesw-resize" };
      name = cur[oct];
    } else if (hit_body(S.fl, x, y)) name = "move";
  }
  set_cursor_name(name);
}

static void on_area_realize(GtkWidget *w, gpointer d) { set_cursor_name("crosshair"); }

static Obj *new_shape_obj(double x, double y, const GdkRGBA *col, int button) {
  Obj *o;
  if (S.shape == SH_LINE || S.shape == SH_CURVE) {
    o = obj_new(O_POINTS);
    o->npts = S.shape == SH_LINE ? 2 : 4;
    for (int i = 0; i < o->npts; i++) { o->pts[2 * i] = x; o->pts[2 * i + 1] = y; }
  } else {
    o = obj_new(O_SHAPE);
    o->x = x; o->y = y;
  }
  o->shape = S.shape;
  o->stroke = *col;
  o->fill = button == 1 ? S.c2 : S.c1;
  o->width = S.sizes[3];
  o->outline = cur_outline();
  o->filled = cur_filled();
  if (!o->outline && !o->filled) o->outline = TRUE;
  return o;
}

static gboolean on_press(GtkWidget *w, GdkEventButton *e, gpointer d) {
  gtk_widget_grab_focus(w);
  double ex = e->x, ey = e->y, x = (ex - PAD) / S.zoom, y = (ey - PAD) / S.zoom;
  if (e->type == GDK_2BUTTON_PRESS) {
    if (S.poly_active) { finish_polygon(); return TRUE; }
    if (S.fl && S.fl->kind == O_TEXT && e->button == 1 && hit_body(S.fl, x, y)) {
      S.drag = D_NONE;
      text_dialog(S.fl, 0, 0, NULL);
    }
    return TRUE;
  }
  if (e->type != GDK_BUTTON_PRESS || (e->button != 1 && e->button != 3)) return FALSE;
  S.btn = e->button;
  S.sx = S.lx = x;
  S.sy = S.ly = y;

  if (S.poly_active) {
    double *v = (double *)S.path->data;
    if (S.path->len >= 6 && hypot(x - v[0], y - v[1]) * S.zoom <= 8) { finish_polygon(); return TRUE; }
    double p[2] = { x, y };
    g_array_append_vals(S.path, p, 2);
    redraw();
    return TRUE;
  }

  if (S.fl && e->button == 1) {
    int h = hit_handle(S.fl, x, y);
    if (h >= 0) {
      S.orig = *S.fl;
      S.handle = h;
      if (S.fl->kind == O_POINTS) S.drag = D_POINT;
      else if (h == 8) S.drag = D_ROTATE;
      else {
        S.drag = D_RESIZE;
        double cx, cy;
        obj_center(S.fl, &cx, &cy);
        to_world(S.fl, cx - HSX[h] * S.fl->w / 2, cy - HSY[h] * S.fl->h / 2, &S.ax, &S.ay);
      }
      return TRUE;
    }
    if (hit_body(S.fl, x, y)) {
      S.orig = *S.fl;
      S.drag = D_MOVE;
      return TRUE;
    }
  }

  int ch = hit_canvas_handle(ex, ey);
  if (ch && e->button == 1) {
    commit();
    S.drag = D_CANVAS;
    S.canvas_handle = ch;
    S.new_w = S.W;
    S.new_h = S.H;
    return TRUE;
  }

  if (S.fl) commit();

  const GdkRGBA *col = (e->button == 1) ? &S.c1 : &S.c2;
  switch (S.tool) {
  case T_SELECT:
    S.drag = D_SELRECT;
    break;
  case T_FREESELECT: {
    S.drag = D_LASSO;
    g_array_set_size(S.path, 0);
    double p[2] = { x, y };
    g_array_append_vals(S.path, p, 2);
    break;
  }
  case T_PENCIL: case T_ERASER: case T_BRUSH:
    push_undo();
    S.drag = D_DRAW;
    stroke_begin(x, y);
    break;
  case T_FILL:
    flood_fill((int)floor(x), (int)floor(y), col);
    break;
  case T_PICKER:
    pick_color((int)floor(x), (int)floor(y), e->button);
    break;
  case T_TEXT:
    text_dialog(NULL, x, y, col);
    break;
  case T_MAGNIFIER: {
    double vx = ex - gtk_adjustment_get_value(hadj()), vy = ey - gtk_adjustment_get_value(vadj());
    zoom_step(e->button == 1 ? 1 : -1, vx, vy);
    break;
  }
  case T_SHAPE:
    if (S.shape == SH_POLYGON) {
      S.poly_active = TRUE;
      g_array_set_size(S.path, 0);
      double p[2] = { x, y };
      g_array_append_vals(S.path, p, 2);
    } else {
      S.fl = new_shape_obj(x, y, col, e->button);
      S.drag = D_CREATE;
      mark_dirty();
    }
    break;
  default:
    break;
  }
  update_status();
  redraw();
  return TRUE;
}

static void resize_obj(double x, double y, gboolean keep) {
  Obj *o = S.fl;
  int h = S.handle, sx = HSX[h], sy = HSY[h];
  double a = S.orig.angle, dx = x - S.ax, dy = y - S.ay;
  double lx = dx * cos(-a) - dy * sin(-a), ly = dx * sin(-a) + dy * cos(-a);
  double nw = sx ? MAX(lx * sx, 2) : S.orig.w, nh = sy ? MAX(ly * sy, 2) : S.orig.h;
  if (keep && sx && sy && S.orig.w > 0 && S.orig.h > 0) {
    double r = S.orig.w / S.orig.h;
    if (nw / nh > r) nw = nh * r; else nh = nw / r;
  }
  double vx = sx * nw / 2, vy = sy * nh / 2;
  double cx = S.ax + vx * cos(a) - vy * sin(a), cy = S.ay + vx * sin(a) + vy * cos(a);
  o->x = cx - nw / 2; o->y = cy - nh / 2; o->w = nw; o->h = nh;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) {
  double ex = e->x, ey = e->y, x = (ex - PAD) / S.zoom, y = (ey - PAD) / S.zoom;
  gboolean shift = (e->state & GDK_SHIFT_MASK) != 0;
  S.mouse_x = x; S.mouse_y = y; S.mouse_in = TRUE;
  switch (S.drag) {
  case D_DRAW:
    stroke_seg(S.lx, S.ly, x, y);
    break;
  case D_CREATE: {
    Obj *o = S.fl;
    double dx = x - S.sx, dy = y - S.sy;
    if (shift) {
      if (o->kind == O_POINTS) {
        double a = atan2(dy, dx), len = hypot(dx, dy);
        a = round(a / (G_PI / 4)) * (G_PI / 4);
        dx = len * cos(a); dy = len * sin(a);
      } else {
        double m = MAX(fabs(dx), fabs(dy));
        dx = dx < 0 ? -m : m;
        dy = dy < 0 ? -m : m;
      }
    }
    if (o->kind == O_POINTS) {
      double ex2 = S.sx + dx, ey2 = S.sy + dy;
      if (o->npts == 2) { o->pts[2] = ex2; o->pts[3] = ey2; }
      else {
        o->pts[2] = S.sx + dx / 3; o->pts[3] = S.sy + dy / 3;
        o->pts[4] = S.sx + dx * 2 / 3; o->pts[5] = S.sy + dy * 2 / 3;
        o->pts[6] = ex2; o->pts[7] = ey2;
      }
    } else {
      o->x = S.sx; o->y = S.sy; o->w = dx; o->h = dy;
    }
    break;
  }
  case D_MOVE:
    if (S.fl->kind == O_POINTS) {
      for (int i = 0; i < S.fl->npts; i++) {
        S.fl->pts[2 * i] = S.orig.pts[2 * i] + (x - S.sx);
        S.fl->pts[2 * i + 1] = S.orig.pts[2 * i + 1] + (y - S.sy);
      }
    } else {
      S.fl->x = S.orig.x + (x - S.sx);
      S.fl->y = S.orig.y + (y - S.sy);
    }
    break;
  case D_POINT:
    S.fl->pts[2 * S.handle] = x;
    S.fl->pts[2 * S.handle + 1] = y;
    break;
  case D_ROTATE: {
    double cx, cy;
    obj_center(&S.orig, &cx, &cy);
    double a = S.orig.angle + atan2(y - cy, x - cx) - atan2(S.sy - cy, S.sx - cx);
    if (shift) a = round(a / (G_PI / 12)) * (G_PI / 12);
    S.fl->angle = a;
    break;
  }
  case D_RESIZE:
    resize_obj(x, y, shift);
    break;
  case D_LASSO: {
    double p[2] = { x, y };
    g_array_append_vals(S.path, p, 2);
    break;
  }
  case D_CANVAS:
    if (S.canvas_handle & 1) S.new_w = MAX(1, round((ex - PAD) / S.zoom));
    if (S.canvas_handle & 2) S.new_h = MAX(1, round((ey - PAD) / S.zoom));
    break;
  case D_NONE:
    update_cursor(x, y, ex, ey);
    break;
  default:
    break;
  }
  S.lx = x; S.ly = y;
  if (S.drag != D_NONE || S.poly_active) redraw();
  update_status();
  update_rulers();
  return TRUE;
}

static void resize_canvas(int nw, int nh);

static gboolean on_release(GtkWidget *w, GdkEventButton *e, gpointer d) {
  double x = (e->x - PAD) / S.zoom, y = (e->y - PAD) / S.zoom;
  switch (S.drag) {
  case D_DRAW:
    stroke_end();
    break;
  case D_CREATE: {
    Obj *o = S.fl;
    if (!o) break;
    if (o->kind == O_POINTS) {
      if (hypot(o->pts[2 * (o->npts - 1)] - o->pts[0], o->pts[2 * (o->npts - 1) + 1] - o->pts[1]) < 2) {
        obj_free(o); S.fl = NULL;
      }
    } else {
      if (o->w < 0) { o->x += o->w; o->w = -o->w; }
      if (o->h < 0) { o->y += o->h; o->h = -o->h; }
      if (o->w < 2 && o->h < 2) { obj_free(o); S.fl = NULL; }
      else { o->w = MAX(o->w, 1); o->h = MAX(o->h, 1); }
    }
    break;
  }
  case D_SELRECT:
    S.drag = D_NONE;
    lift_rect((int)floor(MIN(S.sx, x)), (int)floor(MIN(S.sy, y)),
              (int)ceil(MAX(S.sx, x)), (int)ceil(MAX(S.sy, y)));
    break;
  case D_LASSO:
    S.drag = D_NONE;
    lift_lasso();
    g_array_set_size(S.path, 0);
    break;
  case D_CANVAS:
    S.drag = D_NONE;
    if ((int)S.new_w != S.W || (int)S.new_h != S.H) resize_canvas((int)S.new_w, (int)S.new_h);
    break;
  default:
    break;
  }
  S.drag = D_NONE;
  update_status();
  redraw();
  return TRUE;
}

static gboolean on_leave(GtkWidget *w, GdkEventCrossing *e, gpointer d) {
  S.mouse_in = FALSE;
  update_status();
  update_rulers();
  return FALSE;
}

static gboolean on_scroll(GtkWidget *w, GdkEventScroll *e, gpointer d) {
  if (!(e->state & GDK_CONTROL_MASK)) return FALSE;
  double vx = e->x - gtk_adjustment_get_value(hadj()), vy = e->y - gtk_adjustment_get_value(vadj());
  double dy = 0;
  if (e->direction == GDK_SCROLL_UP) dy = -1;
  else if (e->direction == GDK_SCROLL_DOWN) dy = 1;
  else if (e->direction == GDK_SCROLL_SMOOTH) dy = e->delta_y;
  if (dy < 0) zoom_step(1, vx, vy);
  else if (dy > 0) zoom_step(-1, vx, vy);
  return TRUE;
}

/* ---------- kanvasni chizish ---------- */

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer d) {
  double z = S.zoom, cw = S.W * z, chh = S.H * z;

  /* soya */
  cairo_rectangle(cr, PAD + 2, PAD + 3, cw, chh);
  cairo_set_source_rgba(cr, 0, 0, 0, 0.22);
  cairo_fill(cr);

  cairo_save(cr);
  cairo_translate(cr, PAD, PAD);
  cairo_scale(cr, z, z);
  cairo_rectangle(cr, 0, 0, S.W, S.H);
  cairo_clip(cr);
  cairo_set_source_surface(cr, S.surf, 0, 0);
  cairo_pattern_set_filter(cairo_get_source(cr), z >= 2 ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_GOOD);
  cairo_paint(cr);
  if (S.layer) {
    cairo_set_source_surface(cr, S.layer, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), z >= 2 ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_GOOD);
    cairo_paint_with_alpha(cr, S.layer_alpha);
  }
  if (S.fl) render_obj(cr, S.fl);
  cairo_restore(cr);

  /* to'r chiziqlar */
  if (S.grid_on) {
    static const int steps[] = { 1, 2, 5, 10, 20, 25, 50, 100, 200, 500, 1000 };
    int st = 1000;
    for (gsize i = 0; i < G_N_ELEMENTS(steps); i++) if (steps[i] * z >= 12) { st = steps[i]; break; }
    cairo_save(cr);
    cairo_rectangle(cr, PAD, PAD, cw, chh);
    cairo_clip(cr);
    cairo_set_line_width(cr, 1);
    cairo_set_source_rgba(cr, 0.4, 0.4, 0.45, 0.35);
    for (int i = st; i < S.W; i += st) {
      double gx = floor(PAD + i * z) + 0.5;
      cairo_move_to(cr, gx, PAD); cairo_line_to(cr, gx, PAD + chh);
    }
    for (int i = st; i < S.H; i += st) {
      double gy = floor(PAD + i * z) + 0.5;
      cairo_move_to(cr, PAD, gy); cairo_line_to(cr, PAD + cw, gy);
    }
    cairo_stroke(cr);
    cairo_restore(cr);
  }

  /* tutqichlar va tanlash izlari */
  cairo_save(cr);
  cairo_translate(cr, PAD, PAD);
  cairo_scale(cr, z, z);
  double dash = 4 / z;
  if (S.fl && S.drag != D_CREATE) draw_handles(cr, S.fl);
  if (S.drag == D_SELRECT) {
    cairo_rectangle(cr, MIN(S.sx, S.lx), MIN(S.sy, S.ly), fabs(S.lx - S.sx), fabs(S.ly - S.sy));
    cairo_set_line_width(cr, 1.2 / z);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_stroke_preserve(cr);
    cairo_set_dash(cr, &dash, 1, 0);
    cairo_set_source_rgb(cr, 0.15, 0.4, 0.95);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);
  }
  if ((S.drag == D_LASSO || S.poly_active) && S.path->len >= 2) {
    double *v = (double *)S.path->data;
    int n = S.path->len / 2;
    cairo_move_to(cr, v[0], v[1]);
    for (int i = 1; i < n; i++) cairo_line_to(cr, v[2 * i], v[2 * i + 1]);
    if (S.poly_active) cairo_line_to(cr, S.lx, S.ly);
    if (S.poly_active) {
      cairo_set_line_width(cr, S.sizes[3]);
      cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
      cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
      gdk_cairo_set_source_rgba(cr, &S.c1);
      cairo_stroke(cr);
      if (n >= 3) handle_dot(cr, v[0], v[1], 5 / z);
    } else {
      cairo_set_line_width(cr, 1.2 / z);
      cairo_set_source_rgb(cr, 1, 1, 1);
      cairo_stroke_preserve(cr);
      cairo_set_dash(cr, &dash, 1, 0);
      cairo_set_source_rgb(cr, 0.15, 0.4, 0.95);
      cairo_stroke(cr);
      cairo_set_dash(cr, NULL, 0, 0);
    }
  }
  cairo_restore(cr);

  /* kanvas o'lchamini o'zgartirish tutqichlari */
  if (S.drag == D_CANVAS) {
    double d2 = 4;
    cairo_rectangle(cr, PAD + 0.5, PAD + 0.5, S.new_w * z, S.new_h * z);
    cairo_set_line_width(cr, 1);
    cairo_set_dash(cr, &d2, 1, 0);
    cairo_set_source_rgb(cr, 0.15, 0.4, 0.95);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);
  }
  cairo_set_line_width(cr, 1);
  double hx[3] = { PAD + cw + 4, PAD + cw / 2, PAD + cw + 4 }, hy[3] = { PAD + chh / 2, PAD + chh + 4, PAD + chh + 4 };
  for (int i = 0; i < 3; i++) {
    cairo_rectangle(cr, floor(hx[i]) - 3.5, floor(hy[i]) - 3.5, 7, 7);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.35, 0.35, 0.4);
    cairo_stroke(cr);
  }
  return FALSE;
}

/* ---------- chizg'ichlar ---------- */

static gboolean draw_ruler(GtkWidget *w, cairo_t *cr, gpointer data) {
  gboolean horiz = GPOINTER_TO_INT(data);
  GtkStyleContext *sc = gtk_widget_get_style_context(w);
  int aw = gtk_widget_get_allocated_width(w), ah = gtk_widget_get_allocated_height(w);
  gtk_render_background(sc, cr, 0, 0, aw, ah);
  GdkRGBA fg;
  gtk_style_context_get_color(sc, gtk_widget_get_state_flags(w), &fg);
  if (!S.scroll) return FALSE;
  double z = S.zoom, off = gtk_adjustment_get_value(horiz ? hadj() : vadj());
  int len = horiz ? aw : ah, th = horiz ? ah : aw;
  static const int steps[] = { 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000 };
  int major = 5000;
  for (gsize i = 0; i < G_N_ELEMENTS(steps); i++) if (steps[i] * z >= 55) { major = steps[i]; break; }
  double minor = major / 10.0;
  if (minor * z < 5) minor = major / 5.0;
  if (minor * z < 5) minor = major / 2.0;
  double i0 = floor(((off - PAD) / z) / minor) * minor, i1 = (off + len - PAD) / z;
  cairo_set_line_width(cr, 1);
  gdk_cairo_set_source_rgba(cr, &fg);
  PangoLayout *l = gtk_widget_create_pango_layout(w, NULL);
  PangoFontDescription *fd = pango_font_description_new();
  pango_font_description_set_size(fd, (int)(MAX(7, S.ui_size * 0.7) * PANGO_SCALE));
  pango_layout_set_font_description(l, fd);
  pango_font_description_free(fd);
  for (double i = i0; i <= i1; i += minor) {
    double pos = floor(PAD + i * z - off) + 0.5;
    long ii = lround(i);
    gboolean is_major = fabs(i - ii) < 1e-6 && ii % major == 0;
    double tl = is_major ? th * 0.6 : th * 0.25;
    if (horiz) { cairo_move_to(cr, pos, th); cairo_line_to(cr, pos, th - tl); }
    else { cairo_move_to(cr, th, pos); cairo_line_to(cr, th - tl, pos); }
    cairo_stroke(cr);
    if (is_major) {
      char b[16];
      g_snprintf(b, sizeof b, "%d", (int)lround(i));
      pango_layout_set_text(l, b, -1);
      if (horiz) {
        cairo_move_to(cr, pos + 2, 0);
        pango_cairo_show_layout(cr, l);
      } else {
        cairo_save(cr);
        cairo_move_to(cr, 0, pos - 2);
        cairo_rotate(cr, -G_PI / 2);
        pango_cairo_show_layout(cr, l);
        cairo_restore(cr);
      }
    }
  }
  g_object_unref(l);
  if (S.mouse_in) {
    double pos = PAD + (horiz ? S.mouse_x : S.mouse_y) * z - off;
    cairo_set_source_rgb(cr, 0.15, 0.45, 0.95);
    cairo_set_line_width(cr, 1.5);
    if (horiz) { cairo_move_to(cr, pos, 0); cairo_line_to(cr, pos, th); }
    else { cairo_move_to(cr, 0, pos); cairo_line_to(cr, th, pos); }
    cairo_stroke(cr);
  }
  return FALSE;
}

/* ---------- kichik oyna (miniatyura) ---------- */

static void thumb_geom(double *sc, double *ox, double *oy) {
  int aw = gtk_widget_get_allocated_width(S.thumb), ah = gtk_widget_get_allocated_height(S.thumb);
  *sc = MIN((aw - 8.0) / S.W, (ah - 8.0) / S.H);
  *ox = (aw - S.W * *sc) / 2;
  *oy = (ah - S.H * *sc) / 2;
}

static gboolean draw_thumb(GtkWidget *w, cairo_t *cr, gpointer d) {
  double sc, ox, oy;
  thumb_geom(&sc, &ox, &oy);
  cairo_save(cr);
  cairo_translate(cr, ox, oy);
  cairo_scale(cr, sc, sc);
  cairo_set_source_surface(cr, S.surf, 0, 0);
  cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
  cairo_paint(cr);
  if (S.fl) render_obj(cr, S.fl);
  cairo_restore(cr);
  double z = S.zoom;
  double vx = (gtk_adjustment_get_value(hadj()) - PAD) / z, vy = (gtk_adjustment_get_value(vadj()) - PAD) / z;
  double vw = gtk_adjustment_get_page_size(hadj()) / z, vh = gtk_adjustment_get_page_size(vadj()) / z;
  cairo_rectangle(cr, ox + MAX(vx, 0) * sc, oy + MAX(vy, 0) * sc, MIN(vw, S.W) * sc, MIN(vh, S.H) * sc);
  cairo_set_source_rgb(cr, 0.15, 0.45, 0.95);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);
  return FALSE;
}

static void thumb_goto(double ex, double ey) {
  double sc, ox, oy;
  thumb_geom(&sc, &ox, &oy);
  double ix = (ex - ox) / sc, iy = (ey - oy) / sc;
  gtk_adjustment_set_value(hadj(), PAD + ix * S.zoom - gtk_adjustment_get_page_size(hadj()) / 2);
  gtk_adjustment_set_value(vadj(), PAD + iy * S.zoom - gtk_adjustment_get_page_size(vadj()) / 2);
}

static gboolean on_thumb_press(GtkWidget *w, GdkEventButton *e, gpointer d) {
  S.drag = D_THUMB;
  thumb_goto(e->x, e->y);
  return TRUE;
}

static gboolean on_thumb_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) {
  if (S.drag == D_THUMB) thumb_goto(e->x, e->y);
  return TRUE;
}

static gboolean on_thumb_release(GtkWidget *w, GdkEventButton *e, gpointer d) {
  S.drag = D_NONE;
  return TRUE;
}

static void on_adj_changed(GtkAdjustment *a, gpointer d) {
  update_rulers();
  if (S.thumb_on && S.thumb) gtk_widget_queue_draw(S.thumb);
}
