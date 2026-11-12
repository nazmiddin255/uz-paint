/*
 * Uz Paint — fayllar, rasm amallari, almashish buferi, chop etish, ish stoli foni.
 * Copyright (c) 2026 Uz Paint mualliflari — MIT litsenziyasi.
 */

static void rebuild_recent_menu(void);

/* ---------- rasm <-> pixbuf ---------- */

static GdkPixbuf *surface_to_pixbuf_rgb(cairo_surface_t *s) {
  int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
  cairo_surface_flush(s);
  GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
  guchar *dst = gdk_pixbuf_get_pixels(pb);
  int ds = gdk_pixbuf_get_rowstride(pb), ss = cairo_image_surface_get_stride(s);
  guint8 *src = cairo_image_surface_get_data(s);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      guint32 p = ((guint32 *)(src + y * ss))[x];
      int a = p >> 24;
      guchar *o = dst + y * ds + x * 3;
      o[0] = (guchar)(((p >> 16) & 255) + 255 - a);
      o[1] = (guchar)(((p >> 8) & 255) + 255 - a);
      o[2] = (guchar)((p & 255) + 255 - a);
    }
  return pb;
}

static cairo_surface_t *pixbuf_to_surface(GdkPixbuf *pb, gboolean white_bg) {
  int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
  cairo_surface_t *s = white_bg ? surf_new(w, h) : cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
  cairo_t *cr = cairo_create(s);
  gdk_cairo_set_source_pixbuf(cr, pb, 0, 0);
  cairo_paint(cr);
  cairo_destroy(cr);
  return s;
}

/* ---------- hujjat ---------- */

static void reset_doc(cairo_surface_t *s, const char *fn) {
  obj_free(S.fl);
  S.fl = NULL;
  S.drag = D_NONE;
  S.poly_active = FALSE;
  if (S.path) g_array_set_size(S.path, 0);
  free_list(&S.undo);
  free_list(&S.redo);
  S.undo_bytes = 0;
  if (S.surf) cairo_surface_destroy(S.surf);
  g_free(S.filename);
  S.filename = fn ? g_strdup(fn) : NULL;
  S.dirty = FALSE;
  S.zoom = 1;
  set_surface(s);
  update_title();
}

static void add_recent(const char *fn) {
  char *abs = g_canonicalize_filename(fn, NULL);
  for (int i = 0; i < S.nrecent; i++)
    if (!g_strcmp0(S.recent[i], abs)) {
      g_free(S.recent[i]);
      for (int j = i; j < S.nrecent - 1; j++) S.recent[j] = S.recent[j + 1];
      S.nrecent--;
      break;
    }
  if (S.nrecent == 8) { g_free(S.recent[7]); S.nrecent--; }
  for (int j = S.nrecent; j > 0; j--) S.recent[j] = S.recent[j - 1];
  S.recent[0] = abs;
  S.nrecent++;
  gtk_recent_manager_add_item(gtk_recent_manager_get_default(), g_filename_to_uri(abs, NULL, NULL));
  rebuild_recent_menu();
  save_settings();
}

static gboolean load_file(const char *fn) {
  GError *err = NULL;
  GdkPixbuf *pb = gdk_pixbuf_new_from_file(fn, &err);
  if (!pb) {
    msg_dialog(GTK_MESSAGE_ERROR, T("err.open"), err ? err->message : NULL);
    g_clear_error(&err);
    return FALSE;
  }
  GdkPixbuf *rot = gdk_pixbuf_apply_embedded_orientation(pb);
  g_object_unref(pb);
  reset_doc(pixbuf_to_surface(rot, TRUE), fn);
  g_object_unref(rot);
  add_recent(fn);
  GtkAllocation a;
  gtk_widget_get_allocation(S.scroll, &a);
  if (a.width > 50 && (S.W + 2 * PAD > a.width || S.H + 2 * PAD > a.height)) zoom_fit();
  return TRUE;
}

static GtkFileFilter *mk_filter(const char *name, const char *mime, const char *pat) {
  GtkFileFilter *f = gtk_file_filter_new();
  gtk_file_filter_set_name(f, name);
  if (mime) gtk_file_filter_add_mime_type(f, mime);
  if (pat) gtk_file_filter_add_pattern(f, pat);
  return f;
}

static const char *SAVE_EXT[] = { ".png", ".jpg", ".bmp", ".tiff" };

static char *choose_file(gboolean save, const char *title_key) {
  GtkFileChooserNative *n = gtk_file_chooser_native_new(T(title_key), GTK_WINDOW(S.win),
      save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
      save ? T("act.save") : T("act.open"), T("ui.cancel"));
  GtkFileChooser *fc = GTK_FILE_CHOOSER(n);
  const char *pics = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
  GtkFileFilter *sf[4] = { NULL };
  if (save) {
    gtk_file_chooser_set_do_overwrite_confirmation(fc, TRUE);
    sf[0] = mk_filter("PNG (*.png)", "image/png", "*.png");
    sf[1] = mk_filter("JPEG (*.jpg, *.jpeg)", "image/jpeg", "*.jpg");
    sf[2] = mk_filter("BMP (*.bmp)", "image/bmp", "*.bmp");
    sf[3] = mk_filter("TIFF (*.tiff)", "image/tiff", "*.tiff");
    for (int i = 0; i < 4; i++) gtk_file_chooser_add_filter(fc, sf[i]);
    if (S.filename) {
      gtk_file_chooser_set_filename(fc, S.filename);
    } else {
      gtk_file_chooser_set_current_folder(fc, pics ? pics : g_get_home_dir());
      char *nm = g_strconcat(T("ui.untitled"), ".png", NULL);
      gtk_file_chooser_set_current_name(fc, nm);
      g_free(nm);
    }
  } else {
    GtkFileFilter *f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, T("ui.images"));
    const char *mimes[] = { "image/png", "image/jpeg", "image/bmp", "image/gif", "image/webp",
                            "image/tiff", "image/x-icon", "image/x-portable-pixmap", NULL };
    for (int i = 0; mimes[i]; i++) gtk_file_filter_add_mime_type(f, mimes[i]);
    const char *pats[] = { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif", "*.webp", "*.tif", "*.tiff", "*.ico", NULL };
    for (int i = 0; pats[i]; i++) gtk_file_filter_add_pattern(f, pats[i]);
    gtk_file_chooser_add_filter(fc, f);
    gtk_file_chooser_add_filter(fc, mk_filter(T("ui.allfiles"), NULL, "*"));
    if (S.filename) {
      char *dir = g_path_get_dirname(S.filename);
      gtk_file_chooser_set_current_folder(fc, dir);
      g_free(dir);
    } else if (pics) gtk_file_chooser_set_current_folder(fc, pics);
  }
  char *r = NULL;
  if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(n)) == GTK_RESPONSE_ACCEPT) {
    r = gtk_file_chooser_get_filename(fc);
    if (save && r) {
      char *lower = g_ascii_strdown(r, -1);
      gboolean has = g_str_has_suffix(lower, ".png") || g_str_has_suffix(lower, ".jpg") ||
                     g_str_has_suffix(lower, ".jpeg") || g_str_has_suffix(lower, ".bmp") ||
                     g_str_has_suffix(lower, ".tif") || g_str_has_suffix(lower, ".tiff");
      g_free(lower);
      if (!has) {
        GtkFileFilter *cur = gtk_file_chooser_get_filter(fc);
        const char *ext = ".png";
        for (int i = 0; i < 4; i++) if (cur == sf[i]) ext = SAVE_EXT[i];
        char *nr = g_strconcat(r, ext, NULL);
        g_free(r);
        r = nr;
      }
    }
  }
  g_object_unref(n);
  return r;
}

static gboolean save_to(const char *fn) {
  commit();
  char *lower = g_ascii_strdown(fn, -1);
  const char *type = "png";
  if (g_str_has_suffix(lower, ".jpg") || g_str_has_suffix(lower, ".jpeg")) type = "jpeg";
  else if (g_str_has_suffix(lower, ".bmp")) type = "bmp";
  else if (g_str_has_suffix(lower, ".tif") || g_str_has_suffix(lower, ".tiff")) type = "tiff";
  g_free(lower);
  GdkPixbuf *pb = surface_to_pixbuf_rgb(S.surf);
  GError *err = NULL;
  gboolean ok = !strcmp(type, "jpeg")
      ? gdk_pixbuf_save(pb, fn, type, &err, "quality", "92", NULL)
      : gdk_pixbuf_save(pb, fn, type, &err, NULL);
  g_object_unref(pb);
  if (!ok) {
    msg_dialog(GTK_MESSAGE_ERROR, T("err.save"), err ? err->message : NULL);
    g_clear_error(&err);
    return FALSE;
  }
  if (S.filename != fn) {
    g_free(S.filename);
    S.filename = g_strdup(fn);
  }
  S.dirty = FALSE;
  update_title();
  update_status();
  add_recent(fn);
  return TRUE;
}

static gboolean do_save_as(void) {
  char *fn = choose_file(TRUE, "act.saveas");
  if (!fn) return FALSE;
  gboolean r = save_to(fn);
  g_free(fn);
  return r;
}

static gboolean do_save(void) {
  if (!S.filename) return do_save_as();
  char *fn = g_strdup(S.filename);
  gboolean r = save_to(fn);
  g_free(fn);
  return r;
}

static gboolean confirm_discard(void) {
  if (S.poly_active) finish_polygon();
  if (!S.dirty) return TRUE;
  GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(S.win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
                                        GTK_BUTTONS_NONE, "%s", T("save.ask"));
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", T("save.lost"));
  gtk_dialog_add_buttons(GTK_DIALOG(d), T("save.dont"), GTK_RESPONSE_NO,
                         T("ui.cancel"), GTK_RESPONSE_CANCEL, T("act.save"), GTK_RESPONSE_YES, NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_YES);
  int r = gtk_dialog_run(GTK_DIALOG(d));
  gtk_widget_destroy(d);
  if (r == GTK_RESPONSE_YES) return do_save();
  return r == GTK_RESPONSE_NO;
}

static void act_new(void) {
  if (!confirm_discard()) return;
  reset_doc(surf_new(1000, 700), NULL);
}

static void act_open(void) {
  if (!confirm_discard()) return;
  char *fn = choose_file(FALSE, "act.open");
  if (fn) load_file(fn);
  g_free(fn);
}

static void on_recent_item(GtkMenuItem *it, gpointer d) {
  const char *fn = g_object_get_data(G_OBJECT(it), "file");
  if (!fn || !confirm_discard()) return;
  char *copy = g_strdup(fn);
  load_file(copy);
  g_free(copy);
}

static void rebuild_recent_menu(void) {
  if (!S.recent_menu) return;
  GList *ch = gtk_container_get_children(GTK_CONTAINER(S.recent_menu));
  for (GList *l = ch; l; l = l->next) gtk_widget_destroy(l->data);
  g_list_free(ch);
  if (S.nrecent == 0) {
    GtkWidget *it = gtk_menu_item_new_with_label(T("act.norecent"));
    gtk_widget_set_sensitive(it, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(S.recent_menu), it);
  }
  for (int i = 0; i < S.nrecent; i++) {
    char *base = g_path_get_basename(S.recent[i]);
    char *lab = g_strdup_printf("%d. %s", i + 1, base);
    GtkWidget *it = gtk_menu_item_new_with_label(lab);
    gtk_widget_set_tooltip_text(it, S.recent[i]);
    g_object_set_data_full(G_OBJECT(it), "file", g_strdup(S.recent[i]), g_free);
    g_signal_connect(it, "activate", G_CALLBACK(on_recent_item), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(S.recent_menu), it);
    g_free(lab);
    g_free(base);
  }
  gtk_widget_show_all(S.recent_menu);
}

/* ---------- rasm amallari ---------- */

static void replace_surface(cairo_surface_t *ns) {
  cairo_surface_destroy(S.surf);
  set_surface(ns);
  mark_dirty();
}

static void resize_canvas(int nw, int nh) {
  nw = CLAMP(nw, 1, 20000);
  nh = CLAMP(nh, 1, 20000);
  push_undo();
  cairo_surface_t *s = surf_new_color(nw, nh, &S.c2);
  cairo_t *cr = cairo_create(s);
  cairo_set_source_surface(cr, S.surf, 0, 0);
  cairo_paint(cr);
  cairo_destroy(cr);
  replace_surface(s);
}

/* op: 0 o'ngga 90, 1 chapga 90, 2 180, 3 gorizontal, 4 vertikal */
static cairo_surface_t *transform_surface(cairo_surface_t *src, int op) {
  int w = cairo_image_surface_get_width(src), h = cairo_image_surface_get_height(src);
  int nw = (op <= 1) ? h : w, nh = (op <= 1) ? w : h;
  cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, nw, nh);
  cairo_t *cr = cairo_create(s);
  switch (op) {
  case 0: cairo_translate(cr, h, 0); cairo_rotate(cr, G_PI / 2); break;
  case 1: cairo_translate(cr, 0, w); cairo_rotate(cr, -G_PI / 2); break;
  case 2: cairo_translate(cr, w, h); cairo_rotate(cr, G_PI); break;
  case 3: cairo_translate(cr, w, 0); cairo_scale(cr, -1, 1); break;
  case 4: cairo_translate(cr, 0, h); cairo_scale(cr, 1, -1); break;
  }
  cairo_set_source_surface(cr, src, 0, 0);
  cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);
  cairo_destroy(cr);
  return s;
}

static void obj_drop_key(Obj *o) {
  if (o->img_key) { cairo_surface_destroy(o->img_key); o->img_key = NULL; }
}

static void image_transform(int op) {
  if (S.poly_active) finish_polygon();
  if (S.fl && S.fl->kind == O_IMAGE) {
    Obj *o = S.fl;
    cairo_surface_t *n = transform_surface(o->img, op);
    cairo_surface_destroy(o->img);
    o->img = n;
    obj_drop_key(o);
    if (op <= 1) {
      double cx = o->x + o->w / 2, cy = o->y + o->h / 2, t = o->w;
      o->w = o->h; o->h = t;
      o->x = cx - o->w / 2; o->y = cy - o->h / 2;
    }
    mark_dirty();
    update_status();
    redraw();
    return;
  }
  commit();
  push_undo();
  replace_surface(transform_surface(S.surf, op));
}

static void act_rot_right(void) { image_transform(0); }
static void act_rot_left(void) { image_transform(1); }
static void act_rot_180(void) { image_transform(2); }
static void act_flip_h(void) { image_transform(3); }
static void act_flip_v(void) { image_transform(4); }

static void invert_surface(cairo_surface_t *s) {
  cairo_surface_flush(s);
  guint8 *d = cairo_image_surface_get_data(s);
  int st = cairo_image_surface_get_stride(s), w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
  for (int y = 0; y < h; y++) {
    guint32 *row = (guint32 *)(d + y * st);
    for (int x = 0; x < w; x++) {
      guint32 p = row[x], a = p >> 24;
      guint32 r = a - ((p >> 16) & 255), g = a - ((p >> 8) & 255), b = a - (p & 255);
      row[x] = (a << 24) | (r << 16) | (g << 8) | b;
    }
  }
  cairo_surface_mark_dirty(s);
}

static void act_invert(void) {
  if (S.fl && S.fl->kind == O_IMAGE) {
    invert_surface(S.fl->img);
    obj_drop_key(S.fl);
    mark_dirty();
    redraw();
    return;
  }
  commit();
  push_undo();
  invert_surface(S.surf);
  redraw();
}

static void grayscale_surface(cairo_surface_t *s) {
  cairo_surface_flush(s);
  guint8 *d = cairo_image_surface_get_data(s);
  int st = cairo_image_surface_get_stride(s), w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
  for (int y = 0; y < h; y++) {
    guint32 *row = (guint32 *)(d + y * st);
    for (int x = 0; x < w; x++) {
      guint32 p = row[x];
      guint32 l = (guint32)(0.299 * ((p >> 16) & 255) + 0.587 * ((p >> 8) & 255) + 0.114 * (p & 255) + 0.5);
      row[x] = (p & 0xFF000000u) | (l << 16) | (l << 8) | l;
    }
  }
  cairo_surface_mark_dirty(s);
}

static void act_crop(void) {
  if (!S.fl) return;
  int ox, oy;
  cairo_surface_t *img = obj_to_surface(S.fl, &ox, &oy);
  if (!S.fl->lifted) push_undo();
  int w = cairo_image_surface_get_width(img), h = cairo_image_surface_get_height(img);
  cairo_surface_t *s = surf_new_color(w, h, &S.c2);
  cairo_t *cr = cairo_create(s);
  cairo_set_source_surface(cr, img, 0, 0);
  cairo_paint(cr);
  cairo_destroy(cr);
  cairo_surface_destroy(img);
  obj_free(S.fl);
  S.fl = NULL;
  replace_surface(s);
  update_status();
}

static void act_clear(void) {
  if (S.poly_active) { S.poly_active = FALSE; g_array_set_size(S.path, 0); }
  if (S.fl) { gboolean lifted = S.fl->lifted; obj_free(S.fl); S.fl = NULL; if (!lifted) push_undo(); }
  else push_undo();
  cairo_t *cr = cairo_create(S.surf);
  gdk_cairo_set_source_rgba(cr, &S.c2);
  cairo_paint(cr);
  cairo_destroy(cr);
  update_status();
  redraw();
}

static cairo_surface_t *scale_surface(cairo_surface_t *src, int nw, int nh) {
  int w = cairo_image_surface_get_width(src), h = cairo_image_surface_get_height(src);
  cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, nw, nh);
  cairo_t *cr = cairo_create(s);
  cairo_scale(cr, (double)nw / w, (double)nh / h);
  cairo_set_source_surface(cr, src, 0, 0);
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
  cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);
  cairo_destroy(cr);
  return s;
}

static cairo_surface_t *skew_surface(cairo_surface_t *src, double dh, double dv, const GdkRGBA *bg) {
  int w = cairo_image_surface_get_width(src), h = cairo_image_surface_get_height(src);
  double th = tan(dh * G_PI / 180), tv = tan(dv * G_PI / 180);
  double xs[4] = { 0, (double)w, 0, (double)w }, ys[4] = { 0, 0, (double)h, (double)h };
  double minx = 1e18, miny = 1e18, maxx = -1e18, maxy = -1e18;
  for (int i = 0; i < 4; i++) {
    double nx = xs[i] + ys[i] * th, ny = ys[i] + xs[i] * tv;
    minx = MIN(minx, nx); maxx = MAX(maxx, nx); miny = MIN(miny, ny); maxy = MAX(maxy, ny);
  }
  int nw = CLAMP((int)ceil(maxx - minx), 1, 20000), nh = CLAMP((int)ceil(maxy - miny), 1, 20000);
  cairo_surface_t *s = bg ? surf_new_color(nw, nh, bg) : cairo_image_surface_create(CAIRO_FORMAT_ARGB32, nw, nh);
  cairo_t *cr = cairo_create(s);
  cairo_matrix_t m;
  cairo_matrix_init(&m, 1, tv, th, 1, -minx, -miny);
  cairo_transform(cr, &m);
  cairo_set_source_surface(cr, src, 0, 0);
  cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
  cairo_paint(cr);
  cairo_destroy(cr);
  return s;
}

/* o'lcham oynasi */
typedef struct { GtkWidget *pct, *h, *v, *keep; double bw, bh; gboolean busy; } ResizeUI;

static void rs_mode(GtkToggleButton *b, ResizeUI *u) {
  gboolean pct = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(u->pct));
  u->busy = TRUE;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(u->h), pct ? 100 : u->bw);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(u->v), pct ? 100 : u->bh);
  u->busy = FALSE;
}

static void rs_changed(GtkSpinButton *sb, ResizeUI *u) {
  if (u->busy || !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(u->keep))) return;
  gboolean pct = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(u->pct));
  u->busy = TRUE;
  double val = gtk_spin_button_get_value(sb);
  if (GTK_WIDGET(sb) == u->h)
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(u->v), pct ? val : round(val * u->bh / u->bw));
  else
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(u->h), pct ? val : round(val * u->bw / u->bh));
  u->busy = FALSE;
}

static GtkWidget *grid_label(const char *t) {
  GtkWidget *l = gtk_label_new(t);
  gtk_widget_set_halign(l, GTK_ALIGN_START);
  return l;
}

static void act_resize(void) {
  if (S.poly_active) finish_polygon();
  gboolean sel = S.fl && S.fl->kind == O_IMAGE;
  if (S.fl && !sel) commit();
  ResizeUI u = { 0 };
  u.bw = sel ? S.fl->w : S.W;
  u.bh = sel ? S.fl->h : S.H;
  GtkWidget *dlg = gtk_dialog_new_with_buttons(T("rs.title"), GTK_WINDOW(S.win),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      T("ui.cancel"), GTK_RESPONSE_CANCEL, T("ui.ok"), GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
  GtkWidget *g = gtk_grid_new();
  gtk_container_set_border_width(GTK_CONTAINER(g), 14);
  gtk_grid_set_row_spacing(GTK_GRID(g), 8);
  gtk_grid_set_column_spacing(GTK_GRID(g), 12);
  GtkWidget *t1 = gtk_label_new(NULL);
  char *mk = g_markup_printf_escaped("<b>%s</b>", T("rs.resize"));
  gtk_label_set_markup(GTK_LABEL(t1), mk);
  g_free(mk);
  gtk_widget_set_halign(t1, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(g), t1, 0, 0, 2, 1);
  u.pct = gtk_radio_button_new_with_label(NULL, T("rs.percent"));
  GtkWidget *px = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(u.pct), T("rs.pixels"));
  GtkWidget *rb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start(GTK_BOX(rb), u.pct, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(rb), px, FALSE, FALSE, 0);
  gtk_grid_attach(GTK_GRID(g), rb, 0, 1, 2, 1);
  u.h = gtk_spin_button_new_with_range(1, 20000, 1);
  u.v = gtk_spin_button_new_with_range(1, 20000, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(u.h), 100);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(u.v), 100);
  gtk_entry_set_activates_default(GTK_ENTRY(u.h), TRUE);
  gtk_entry_set_activates_default(GTK_ENTRY(u.v), TRUE);
  gtk_grid_attach(GTK_GRID(g), grid_label(T("rs.horiz")), 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(g), u.h, 1, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(g), grid_label(T("rs.vert")), 0, 3, 1, 1);
  gtk_grid_attach(GTK_GRID(g), u.v, 1, 3, 1, 1);
  u.keep = gtk_check_button_new_with_label(T("rs.keep"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(u.keep), TRUE);
  gtk_grid_attach(GTK_GRID(g), u.keep, 0, 4, 2, 1);
  GtkWidget *t2 = gtk_label_new(NULL);
  mk = g_markup_printf_escaped("<b>%s</b>", T("rs.skew"));
  gtk_label_set_markup(GTK_LABEL(t2), mk);
  g_free(mk);
  gtk_widget_set_halign(t2, GTK_ALIGN_START);
  gtk_widget_set_margin_top(t2, 8);
  gtk_grid_attach(GTK_GRID(g), t2, 0, 5, 2, 1);
  GtkWidget *kh = gtk_spin_button_new_with_range(-89, 89, 1), *kv = gtk_spin_button_new_with_range(-89, 89, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(kh), 0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(kv), 0);
  gtk_entry_set_activates_default(GTK_ENTRY(kh), TRUE);
  gtk_entry_set_activates_default(GTK_ENTRY(kv), TRUE);
  gtk_grid_attach(GTK_GRID(g), grid_label(T("rs.horiz")), 0, 6, 1, 1);
  gtk_grid_attach(GTK_GRID(g), kh, 1, 6, 1, 1);
  gtk_grid_attach(GTK_GRID(g), grid_label(T("rs.vert")), 0, 7, 1, 1);
  gtk_grid_attach(GTK_GRID(g), kv, 1, 7, 1, 1);
  g_signal_connect(u.pct, "toggled", G_CALLBACK(rs_mode), &u);
  g_signal_connect(u.h, "value-changed", G_CALLBACK(rs_changed), &u);
  g_signal_connect(u.v, "value-changed", G_CALLBACK(rs_changed), &u);
  gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), g);
  gtk_widget_show_all(dlg);
  if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
    gboolean pct = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(u.pct));
    double hv = gtk_spin_button_get_value(GTK_SPIN_BUTTON(u.h)), vv = gtk_spin_button_get_value(GTK_SPIN_BUTTON(u.v));
    double nw = pct ? u.bw * hv / 100 : hv, nh = pct ? u.bh * vv / 100 : vv;
    double sh = gtk_spin_button_get_value(GTK_SPIN_BUTTON(kh)), sv = gtk_spin_button_get_value(GTK_SPIN_BUTTON(kv));
    nw = CLAMP(round(nw), 1, 20000); nh = CLAMP(round(nh), 1, 20000);
    if (sel) {
      Obj *o = S.fl;
      if (fabs(sh) > 0.01 || fabs(sv) > 0.01) {
        cairo_surface_t *sc = scale_surface(o->img, (int)nw, (int)nh);
        cairo_surface_t *sk = skew_surface(sc, sh, sv, NULL);
        cairo_surface_destroy(sc);
        cairo_surface_destroy(o->img);
        o->img = sk;
        nw = cairo_image_surface_get_width(sk);
        nh = cairo_image_surface_get_height(sk);
        obj_drop_key(o);
      }
      o->w = nw; o->h = nh;
      mark_dirty();
      update_status();
      redraw();
    } else if ((int)nw != S.W || (int)nh != S.H || fabs(sh) > 0.01 || fabs(sv) > 0.01) {
      push_undo();
      cairo_surface_t *s = ((int)nw != S.W || (int)nh != S.H) ? scale_surface(S.surf, (int)nw, (int)nh) : surf_copy(S.surf);
      if (fabs(sh) > 0.01 || fabs(sv) > 0.01) {
        cairo_surface_t *k = skew_surface(s, sh, sv, &S.c2);
        cairo_surface_destroy(s);
        s = k;
      }
      replace_surface(s);
    }
  }
  gtk_widget_destroy(dlg);
}

/* xususiyatlar */
static void act_properties(void) {
  commit();
  GtkWidget *dlg = gtk_dialog_new_with_buttons(T("pr.title"), GTK_WINDOW(S.win),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      T("ui.cancel"), GTK_RESPONSE_CANCEL, T("ui.ok"), GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
  GtkWidget *g = gtk_grid_new();
  gtk_container_set_border_width(GTK_CONTAINER(g), 14);
  gtk_grid_set_row_spacing(GTK_GRID(g), 8);
  gtk_grid_set_column_spacing(GTK_GRID(g), 14);
  char *name = S.filename ? g_path_get_basename(S.filename) : g_strdup(T("ui.untitled"));
  char *date = g_strdup(T("pr.notsaved")), *size = g_strdup("—");
  GStatBuf st;
  if (S.filename && g_stat(S.filename, &st) == 0) {
    GDateTime *dt = g_date_time_new_from_unix_local(st.st_mtime);
    g_free(date);
    date = g_date_time_format(dt, "%d.%m.%Y  %H:%M");
    g_date_time_unref(dt);
    g_free(size);
    size = g_format_size(st.st_size);
  }
  gtk_grid_attach(GTK_GRID(g), grid_label(T("pr.file")), 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(g), grid_label(name), 1, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(g), grid_label(T("pr.date")), 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(g), grid_label(date), 1, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(g), grid_label(T("pr.disk")), 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(g), grid_label(size), 1, 2, 1, 1);
  g_free(name); g_free(date); g_free(size);
  GtkWidget *t = gtk_label_new(NULL);
  char *mk = g_markup_printf_escaped("<b>%s</b>", T("pr.canvas"));
  gtk_label_set_markup(GTK_LABEL(t), mk);
  g_free(mk);
  gtk_widget_set_halign(t, GTK_ALIGN_START);
  gtk_widget_set_margin_top(t, 8);
  gtk_grid_attach(GTK_GRID(g), t, 0, 3, 2, 1);
  GtkWidget *sw = gtk_spin_button_new_with_range(1, 20000, 1), *sh = gtk_spin_button_new_with_range(1, 20000, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(sw), S.W);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(sh), S.H);
  gtk_entry_set_activates_default(GTK_ENTRY(sw), TRUE);
  gtk_entry_set_activates_default(GTK_ENTRY(sh), TRUE);
  gtk_grid_attach(GTK_GRID(g), grid_label(T("pr.width")), 0, 4, 1, 1);
  gtk_grid_attach(GTK_GRID(g), sw, 1, 4, 1, 1);
  gtk_grid_attach(GTK_GRID(g), grid_label(T("pr.height")), 0, 5, 1, 1);
  gtk_grid_attach(GTK_GRID(g), sh, 1, 5, 1, 1);
  gtk_grid_attach(GTK_GRID(g), grid_label(T("pr.colors")), 0, 6, 1, 1);
  GtkWidget *rc = gtk_radio_button_new_with_label(NULL, T("pr.color"));
  GtkWidget *rbw = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rc), T("pr.bw"));
  GtkWidget *rb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start(GTK_BOX(rb), rc, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(rb), rbw, FALSE, FALSE, 0);
  gtk_grid_attach(GTK_GRID(g), rb, 1, 6, 1, 1);
  gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), g);
  gtk_widget_show_all(dlg);
  if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
    int nw = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sw)), nh = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sh));
    if (nw != S.W || nh != S.H) resize_canvas(nw, nh);
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rbw))) {
      push_undo();
      grayscale_surface(S.surf);
      redraw();
    }
  }
  gtk_widget_destroy(dlg);
}

/* ---------- almashish buferi ---------- */

static void act_copy(void) {
  cairo_surface_t *s;
  if (S.fl) {
    s = obj_to_surface(S.fl, NULL, NULL);
  } else {
    s = surf_copy(S.surf);
  }
  int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
  GdkPixbuf *pb = gdk_pixbuf_get_from_surface(s, 0, 0, w, h);
  gtk_clipboard_set_image(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), pb);
  g_object_unref(pb);
  cairo_surface_destroy(s);
}

static void act_cut(void) {
  if (!S.fl) return;
  act_copy();
  delete_selection();
}

static void paste_pixbuf(GdkPixbuf *pb) {
  commit();
  if (S.poly_active) finish_polygon();
  int pw = gdk_pixbuf_get_width(pb), ph = gdk_pixbuf_get_height(pb);
  if (pw > S.W || ph > S.H) resize_canvas(MAX(pw, S.W), MAX(ph, S.H));
  if (S.tool != T_SELECT && S.tool != T_FREESELECT) select_tool(T_SELECT, 0);
  Obj *o = obj_new(O_IMAGE);
  o->img = pixbuf_to_surface(pb, FALSE);
  o->w = pw;
  o->h = ph;
  o->x = MAX(0, floor((gtk_adjustment_get_value(hadj()) - PAD) / S.zoom));
  o->y = MAX(0, floor((gtk_adjustment_get_value(vadj()) - PAD) / S.zoom));
  if (o->x + pw > S.W) o->x = MAX(0, S.W - pw);
  if (o->y + ph > S.H) o->y = MAX(0, S.H - ph);
  S.fl = o;
  mark_dirty();
  update_status();
  redraw();
}

static void act_paste(void) {
  GdkPixbuf *pb = gtk_clipboard_wait_for_image(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
  if (!pb) { msg_dialog(GTK_MESSAGE_INFO, T("err.noimage"), NULL); return; }
  paste_pixbuf(pb);
  g_object_unref(pb);
}

static void act_paste_from(void) {
  char *fn = choose_file(FALSE, "act.pastefrom");
  if (!fn) return;
  GError *err = NULL;
  GdkPixbuf *pb = gdk_pixbuf_new_from_file(fn, &err);
  if (pb) {
    GdkPixbuf *r = gdk_pixbuf_apply_embedded_orientation(pb);
    paste_pixbuf(r);
    g_object_unref(r);
    g_object_unref(pb);
  } else {
    msg_dialog(GTK_MESSAGE_ERROR, T("err.open"), err ? err->message : NULL);
    g_clear_error(&err);
  }
  g_free(fn);
}

/* ---------- chop etish ---------- */

static void on_draw_page(GtkPrintOperation *op, GtkPrintContext *ctx, int page, gpointer d) {
  cairo_t *cr = gtk_print_context_get_cairo_context(ctx);
  double pw = gtk_print_context_get_width(ctx), ph = gtk_print_context_get_height(ctx);
  double sc = MIN(0.75, MIN(pw / S.W, ph / S.H));
  cairo_translate(cr, (pw - S.W * sc) / 2, (ph - S.H * sc) / 2);
  cairo_scale(cr, sc, sc);
  cairo_set_source_surface(cr, S.surf, 0, 0);
  cairo_paint(cr);
}

static void act_print(void) {
  commit();
  GtkPrintOperation *op = gtk_print_operation_new();
  gtk_print_operation_set_n_pages(op, 1);
  gtk_print_operation_set_unit(op, GTK_UNIT_POINTS);
  char *nm = S.filename ? g_path_get_basename(S.filename) : g_strdup(T("ui.untitled"));
  gtk_print_operation_set_job_name(op, nm);
  g_free(nm);
  g_signal_connect(op, "draw-page", G_CALLBACK(on_draw_page), NULL);
  GError *err = NULL;
  GtkPrintOperationResult r = gtk_print_operation_run(op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, GTK_WINDOW(S.win), &err);
  if (r == GTK_PRINT_OPERATION_RESULT_ERROR) {
    msg_dialog(GTK_MESSAGE_ERROR, T("err.print"), err ? err->message : NULL);
    g_clear_error(&err);
  }
  g_object_unref(op);
}

/* ---------- ish stoli foni ---------- */

static gboolean try_gsettings(const char *schema_id, const char *key, const char *value) {
  GSettingsSchemaSource *src = g_settings_schema_source_get_default();
  if (!src) return FALSE;
  GSettingsSchema *sch = g_settings_schema_source_lookup(src, schema_id, TRUE);
  if (!sch) return FALSE;
  gboolean ok = FALSE;
  if (g_settings_schema_has_key(sch, key)) {
    GSettings *gs = g_settings_new(schema_id);
    ok = g_settings_set_string(gs, key, value);
    g_settings_sync();
    g_object_unref(gs);
  }
  g_settings_schema_unref(sch);
  return ok;
}

static void act_wallpaper(void) {
  commit();
  char *dir = g_build_filename(g_get_user_data_dir(), APP_ID, NULL);
  g_mkdir_with_parents(dir, 0755);
  /* eski fon fayllarini tozalaymiz */
  GDir *gd = g_dir_open(dir, 0, NULL);
  if (gd) {
    const char *nm;
    while ((nm = g_dir_read_name(gd)))
      if (g_str_has_prefix(nm, "fon-")) { char *p = g_build_filename(dir, nm, NULL); g_unlink(p); g_free(p); }
    g_dir_close(gd);
  }
  char *name = g_strdup_printf("fon-%" G_GINT64_FORMAT ".png", g_get_real_time() / 1000);
  char *path = g_build_filename(dir, name, NULL);
  GdkPixbuf *pb = surface_to_pixbuf_rgb(S.surf);
  gboolean saved = gdk_pixbuf_save(pb, path, "png", NULL, NULL);
  g_object_unref(pb);
  if (!saved) { msg_dialog(GTK_MESSAGE_ERROR, T("err.save"), path); goto out; }
  char *uri = g_filename_to_uri(path, NULL, NULL);
  gboolean ok = FALSE;
  ok |= try_gsettings("org.gnome.desktop.background", "picture-uri", uri);
  ok |= try_gsettings("org.gnome.desktop.background", "picture-uri-dark", uri);
  ok |= try_gsettings("org.cinnamon.desktop.background", "picture-uri", uri);
  ok |= try_gsettings("org.mate.background", "picture-filename", path);
  g_free(uri);
  if (ok) msg_dialog(GTK_MESSAGE_INFO, T("ok.wallpaper"), NULL);
  else msg_dialog(GTK_MESSAGE_WARNING, T("err.wallpaper"), path);
out:
  g_free(path);
  g_free(name);
  g_free(dir);
}
