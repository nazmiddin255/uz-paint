/*
 * Uz Paint — sozlamalar, mavzu, til, menyular, lenta (ribbon) va oyna.
 * Copyright (c) 2026 Uz Paint mualliflari — MIT litsenziyasi.
 */

static const char *PALETTE[2][10] = {
  { "#000000", "#5f6368", "#8b1a1a", "#e53935", "#fb8c00", "#fdd835", "#43a047", "#039be5", "#3949ab", "#8e24aa" },
  { "#ffffff", "#bdbdbd", "#8d6e63", "#f48fb1", "#ffb300", "#fff59d", "#c0ca33", "#81d4fa", "#7986cb", "#ce93d8" } };

static GSList *tool_group;
static GtkCssProvider *css_colors;
static GDBusConnection *dbus;
static int sys_scheme = -1;          /* -1 noma'lum, 0 tanlanmagan, 1 qorong'i, 2 yorug' */
static gboolean orig_prefer_dark;
static int lang_pending = -1;        /* qayta ochilganda qo'llanadigan til */

static gboolean cur_outline(void) { return S.outline_combo ? gtk_combo_box_get_active(GTK_COMBO_BOX(S.outline_combo)) != 0 : TRUE; }
static gboolean cur_filled(void) { return S.fill_combo ? gtk_combo_box_get_active(GTK_COMBO_BOX(S.fill_combo)) != 0 : FALSE; }

/* ---------- sozlamalar fayli ---------- */

static char *settings_path(void) {
  return g_build_filename(g_get_user_config_dir(), APP_ID, "sozlamalar.ini", NULL);
}

static int kf_int(GKeyFile *k, const char *g, const char *key, int def, int lo, int hi) {
  GError *e = NULL;
  int v = g_key_file_get_integer(k, g, key, &e);
  if (e) { g_error_free(e); return def; }
  return CLAMP(v, lo, hi);
}

static void load_settings(void) {
  S.ui_family = g_strdup("");
  S.ui_size = 13;
  S.text_font = g_strdup("Sans 28");
  S.status_on = TRUE;
  S.win_w = 1280; S.win_h = 860;
  GKeyFile *k = g_key_file_new();
  char *p = settings_path();
  if (g_key_file_load_from_file(k, p, G_KEY_FILE_NONE, NULL)) {
    char *f = g_key_file_get_string(k, "Interfeys", "shrift", NULL);
    if (f) { g_free(S.ui_family); S.ui_family = f; }
    S.ui_size = kf_int(k, "Interfeys", "olcham", 13, 8, 32);
    S.theme = kf_int(k, "Interfeys", "mavzu", 0, 0, 2);
    S.lang = kf_int(k, "Interfeys", "til", 0, 0, 1);
    S.rulers = kf_int(k, "Korinish", "chizgichlar", 0, 0, 1);
    S.grid_on = kf_int(k, "Korinish", "tor", 0, 0, 1);
    S.status_on = kf_int(k, "Korinish", "holat", 1, 0, 1);
    S.thumb_on = kf_int(k, "Korinish", "kichik_oyna", 0, 0, 1);
    S.win_w = kf_int(k, "Oyna", "eni", 1280, 500, 10000);
    S.win_h = kf_int(k, "Oyna", "boyi", 860, 400, 10000);
    char *t = g_key_file_get_string(k, "Matn", "shrift", NULL);
    if (t && *t) { g_free(S.text_font); S.text_font = t; } else g_free(t);
    gsize n = 0;
    char **cc = g_key_file_get_string_list(k, "Ranglar", "maxsus", &n, NULL);
    for (gsize i = 0; cc && i < n && S.ncustom < 10; i++)
      if (gdk_rgba_parse(&S.custom[S.ncustom], cc[i])) S.ncustom++;
    g_strfreev(cc);
    char **rr = g_key_file_get_string_list(k, "Fayllar", "oxirgi", &n, NULL);
    for (gsize i = 0; rr && i < n && S.nrecent < 8; i++)
      if (g_file_test(rr[i], G_FILE_TEST_EXISTS)) S.recent[S.nrecent++] = g_strdup(rr[i]);
    g_strfreev(rr);
  }
  g_key_file_free(k);
  g_free(p);
}

static void save_settings(void) {
  GKeyFile *k = g_key_file_new();
  g_key_file_set_string(k, "Interfeys", "shrift", S.ui_family ? S.ui_family : "");
  g_key_file_set_integer(k, "Interfeys", "olcham", S.ui_size);
  g_key_file_set_integer(k, "Interfeys", "mavzu", S.theme);
  g_key_file_set_integer(k, "Interfeys", "til", lang_pending >= 0 ? lang_pending : S.lang);
  g_key_file_set_integer(k, "Korinish", "chizgichlar", S.rulers);
  g_key_file_set_integer(k, "Korinish", "tor", S.grid_on);
  g_key_file_set_integer(k, "Korinish", "holat", S.status_on);
  g_key_file_set_integer(k, "Korinish", "kichik_oyna", S.thumb_on);
  g_key_file_set_integer(k, "Oyna", "eni", S.win_w);
  g_key_file_set_integer(k, "Oyna", "boyi", S.win_h);
  g_key_file_set_string(k, "Matn", "shrift", S.text_font ? S.text_font : "Sans 28");
  char *cc[11];
  for (int i = 0; i < S.ncustom; i++) cc[i] = gdk_rgba_to_string(&S.custom[i]);
  cc[S.ncustom] = NULL;
  g_key_file_set_string_list(k, "Ranglar", "maxsus", (const char *const *)cc, S.ncustom);
  for (int i = 0; i < S.ncustom; i++) g_free(cc[i]);
  g_key_file_set_string_list(k, "Fayllar", "oxirgi", (const char *const *)S.recent, S.nrecent);
  char *p = settings_path(), *dir = g_path_get_dirname(p);
  g_mkdir_with_parents(dir, 0755);
  g_key_file_save_to_file(k, p, NULL);
  g_free(dir);
  g_free(p);
  g_key_file_free(k);
}

/* ---------- mavzu (yorug' / qorong'i) ---------- */

static char *strip_dark(const char *name) {
  char *s = g_strdup(name ? name : "Adwaita");
  char *low = g_ascii_strdown(s, -1);
  if (g_str_has_suffix(low, "-dark")) s[strlen(s) - 5] = 0;
  else if (g_str_has_suffix(low, "_dark")) s[strlen(s) - 5] = 0;
  g_free(low);
  return s;
}

static gboolean theme_exists(const char *name) {
  if (!name || !*name) return FALSE;
  if (!g_ascii_strcasecmp(name, "Adwaita") || !g_ascii_strcasecmp(name, "HighContrast")) return TRUE;
  char *dirs[4] = {
    g_build_filename(g_get_user_data_dir(), "themes", name, "gtk-3.0", NULL),
    g_build_filename(g_get_home_dir(), ".themes", name, "gtk-3.0", NULL),
    g_build_filename("/usr/share/themes", name, "gtk-3.0", NULL),
    g_build_filename("/usr/local/share/themes", name, "gtk-3.0", NULL) };
  gboolean ok = FALSE;
  for (int i = 0; i < 4; i++) {
    if (!ok && g_file_test(dirs[i], G_FILE_TEST_IS_DIR)) ok = TRUE;
    g_free(dirs[i]);
  }
  return ok;
}

static void apply_theme(void) {
  GtkSettings *gs = gtk_settings_get_default();
  if (S.theme == 0 && sys_scheme != 1 && sys_scheme != 2) {
    g_object_set(gs, "gtk-theme-name", S.orig_theme, "gtk-application-prefer-dark-theme", orig_prefer_dark, NULL);
  } else {
    gboolean dark = S.theme == 2 || (S.theme == 0 && sys_scheme == 1);
    char *base = strip_dark(S.orig_theme);
    if (!theme_exists(base)) { g_free(base); base = g_strdup("Adwaita"); }
    g_object_set(gs, "gtk-theme-name", base, "gtk-application-prefer-dark-theme", dark, NULL);
    g_free(base);
  }
  for (guint i = 0; S.icons && i < S.icons->len; i++) gtk_widget_queue_draw(g_ptr_array_index(S.icons, i));
}

static int variant_scheme(GVariant *v) {
  GVariant *cur = g_variant_ref(v);
  while (g_variant_is_of_type(cur, G_VARIANT_TYPE_VARIANT)) {
    GVariant *in = g_variant_get_variant(cur);
    g_variant_unref(cur);
    cur = in;
  }
  int r = g_variant_is_of_type(cur, G_VARIANT_TYPE_UINT32) ? (int)g_variant_get_uint32(cur) : -1;
  g_variant_unref(cur);
  return r;
}

static void on_portal_signal(GDBusConnection *c, const char *sender, const char *path, const char *iface,
                             const char *sig, GVariant *params, gpointer d) {
  const char *ns, *key;
  GVariant *v;
  if (!g_variant_is_of_type(params, G_VARIANT_TYPE("(ssv)"))) return;
  g_variant_get(params, "(&s&sv)", &ns, &key, &v);
  if (!g_strcmp0(ns, "org.freedesktop.appearance") && !g_strcmp0(key, "color-scheme")) {
    sys_scheme = variant_scheme(v);
    if (S.theme == 0) apply_theme();
  }
  g_variant_unref(v);
}

static void detect_system_theme(void) {
  dbus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
  if (dbus) {
    GVariant *r = g_dbus_connection_call_sync(dbus, "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Settings", "Read",
        g_variant_new("(ss)", "org.freedesktop.appearance", "color-scheme"),
        NULL, G_DBUS_CALL_FLAGS_NONE, 700, NULL, NULL);
    if (r) {
      GVariant *v = g_variant_get_child_value(r, 0);
      sys_scheme = variant_scheme(v);
      g_variant_unref(v);
      g_variant_unref(r);
    }
    g_dbus_connection_signal_subscribe(dbus, NULL, "org.freedesktop.portal.Settings", "SettingChanged",
        "/org/freedesktop/portal/desktop", NULL, G_DBUS_SIGNAL_FLAGS_NONE, on_portal_signal, NULL, NULL);
  }
  if (sys_scheme < 0) {
    GSettingsSchemaSource *src = g_settings_schema_source_get_default();
    GSettingsSchema *sch = src ? g_settings_schema_source_lookup(src, "org.gnome.desktop.interface", TRUE) : NULL;
    if (sch) {
      if (g_settings_schema_has_key(sch, "color-scheme")) {
        GSettings *g = g_settings_new("org.gnome.desktop.interface");
        char *v = g_settings_get_string(g, "color-scheme");
        sys_scheme = !g_strcmp0(v, "prefer-dark") ? 1 : !g_strcmp0(v, "prefer-light") ? 2 : 0;
        g_free(v);
        g_object_unref(g);
      }
      g_settings_schema_unref(sch);
    }
  }
}

/* ---------- interfeys shrifti ---------- */

static void apply_ui_font(void) {
  GString *css = g_string_new(NULL);
  g_string_append_printf(css, "* { font-size: %dpt; ", S.ui_size);
  if (S.ui_family && *S.ui_family) g_string_append_printf(css, "font-family: \"%s\"; ", S.ui_family);
  g_string_append(css, "}\n");
  gtk_css_provider_load_from_data(S.css, css->str, -1, NULL);
  g_string_free(css, TRUE);
  int px = (int)lround(S.ui_size * 1.75);
  for (guint i = 0; i < S.icons->len; i++) {
    GtkWidget *w = g_ptr_array_index(S.icons, i);
    int sc = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "scale"));
    int s = (int)lround(px * (sc ? sc : 100) / 100.0);
    gtk_widget_set_size_request(w, s, s);
  }
  int cell = (int)lround(S.ui_size * 1.5);
  if (S.palette) gtk_widget_set_size_request(S.palette, 10 * cell + 1, 3 * cell + 1);
  int rt = (int)lround(S.ui_size * 1.7);
  if (S.hruler) {
    gtk_widget_set_size_request(S.hruler, -1, rt);
    gtk_widget_set_size_request(S.vruler, rt, -1);
    gtk_widget_set_size_request(S.corner, rt, rt);
  }
  if (S.thumb) gtk_widget_set_size_request(S.thumb, S.ui_size * 16, S.ui_size * 11);
  save_settings();
}

static void on_font_family(GtkFontButton *b, GtkWidget *spin) {
  PangoFontDescription *fd = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(b));
  if (!fd) return;
  const char *fam = pango_font_description_get_family(fd);
  g_free(S.ui_family);
  S.ui_family = g_strdup(fam ? fam : "");
  int sz = pango_font_description_get_size(fd) / PANGO_SCALE;
  pango_font_description_free(fd);
  if (sz >= 8 && sz <= 32) gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), sz);
  apply_ui_font();
}

static void on_font_size(GtkSpinButton *sb, gpointer d) {
  S.ui_size = gtk_spin_button_get_value_as_int(sb);
  if (g_getenv("UZ_PAINT_DEBUG")) g_printerr("ui_size=%d\n", S.ui_size);
  apply_ui_font();
}

static void on_font_reset(GtkButton *b, GtkWidget *spin) {
  g_free(S.ui_family);
  S.ui_family = g_strdup("");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 13);
  S.ui_size = 13;
  apply_ui_font();
}

static void act_ui_font(void) {
  GtkWidget *dlg = gtk_dialog_new_with_buttons(T("set.fonttitle"), GTK_WINDOW(S.win),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, T("ui.close"), GTK_RESPONSE_CLOSE, NULL);
  gtk_window_set_default_size(GTK_WINDOW(dlg), 34 * 18, -1);
  GtkWidget *g = gtk_grid_new();
  gtk_container_set_border_width(GTK_CONTAINER(g), 14);
  gtk_grid_set_row_spacing(GTK_GRID(g), 10);
  gtk_grid_set_column_spacing(GTK_GRID(g), 12);
  char *fam = (S.ui_family && *S.ui_family) ? g_strdup(S.ui_family) : NULL;
  if (!fam) {
    char *sys = NULL;
    g_object_get(gtk_settings_get_default(), "gtk-font-name", &sys, NULL);
    PangoFontDescription *fd = pango_font_description_from_string(sys ? sys : "Sans");
    fam = g_strdup(pango_font_description_get_family(fd));
    pango_font_description_free(fd);
    g_free(sys);
  }
  char *fstr = g_strdup_printf("%s %d", fam ? fam : "Sans", S.ui_size);
  GtkWidget *fb = gtk_font_button_new_with_font(fstr);
  g_free(fstr);
  g_free(fam);
  GtkWidget *spin = gtk_spin_button_new_with_range(8, 32, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), S.ui_size);
  g_signal_connect(fb, "font-set", G_CALLBACK(on_font_family), spin);
  g_signal_connect(spin, "value-changed", G_CALLBACK(on_font_size), NULL);
  gtk_grid_attach(GTK_GRID(g), grid_label(T("set.family")), 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(g), fb, 1, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(g), grid_label(T("set.size")), 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(g), spin, 1, 1, 1, 1);
  GtkWidget *pv = gtk_label_new(T("set.preview"));
  gtk_widget_set_margin_top(pv, 6);
  gtk_grid_attach(GTK_GRID(g), pv, 0, 2, 2, 1);
  GtkWidget *rb = gtk_button_new_with_label(T("set.reset"));
  g_signal_connect(rb, "clicked", G_CALLBACK(on_font_reset), spin);
  gtk_grid_attach(GTK_GRID(g), rb, 0, 3, 2, 1);
  gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), g);
  gtk_widget_show_all(dlg);
  gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
}

/* ---------- til ---------- */

static void restart_app(void) {
  if (!confirm_discard()) return;
  save_settings();
  char *exe = g_file_read_link("/proc/self/exe", NULL);
  char *argv[3] = { exe, (S.filename && !S.dirty) ? S.filename : NULL, NULL };
  if (exe) g_spawn_async(NULL, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, NULL);
  g_free(exe);
  S.dirty = FALSE;
  gtk_widget_destroy(S.win);
}

static void on_lang(GtkCheckMenuItem *it, gpointer d) {
  if (S.updating || !gtk_check_menu_item_get_active(it)) return;
  int lang = GPOINTER_TO_INT(d);
  lang_pending = lang;
  save_settings();
  if (lang == S.lang) return;
  GtkWidget *dl = gtk_message_dialog_new(GTK_WINDOW(S.win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
                                         GTK_BUTTONS_NONE, "%s",
                                         lang == 1 ? "The language will change after restart. Restart now?"
                                                   : "Til dastur qayta oçilganda almaşadi. Hozir qayta oçilsinmi?");
  gtk_dialog_add_buttons(GTK_DIALOG(dl), lang == 1 ? "Later" : "Keyinroq", GTK_RESPONSE_NO,
                         lang == 1 ? "Restart" : "Qayta oçiş", GTK_RESPONSE_YES, NULL);
  int r = gtk_dialog_run(GTK_DIALOG(dl));
  gtk_widget_destroy(dl);
  if (r == GTK_RESPONSE_YES) restart_app();
}

static void on_theme(GtkCheckMenuItem *it, gpointer d) {
  if (S.updating || !gtk_check_menu_item_get_active(it)) return;
  S.theme = GPOINTER_TO_INT(d);
  apply_theme();
  save_settings();
}

/* ---------- ranglar ---------- */

static void add_custom_color(const GdkRGBA *c) {
  for (int i = 0; i < S.ncustom; i++) if (gdk_rgba_equal(&S.custom[i], c)) return;
  for (gsize r = 0; r < 2; r++)
    for (int i = 0; i < 10; i++) {
      GdkRGBA p;
      gdk_rgba_parse(&p, PALETTE[r][i]);
      if (gdk_rgba_equal(&p, c)) return;
    }
  if (S.ncustom == 10) S.ncustom--;
  for (int i = S.ncustom; i > 0; i--) S.custom[i] = S.custom[i - 1];
  S.custom[0] = *c;
  S.ncustom++;
  if (S.palette) gtk_widget_queue_draw(S.palette);
  save_settings();
}

static void set_color(int which, const GdkRGBA *c) {
  GdkRGBA cc = *c;
  cc.alpha = 1;
  if (which == 1) {
    S.c1 = cc;
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(S.cbtn1), &cc);
  } else {
    S.c2 = cc;
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(S.cbtn2), &cc);
  }
  if (S.fl && S.fl->kind != O_IMAGE) {
    if (which == 1) S.fl->stroke = cc; else S.fl->fill = cc;
  }
  redraw();
}

static void on_color_set(GtkColorButton *b, gpointer d) {
  GdkRGBA c;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(b), &c);
  set_color(GPOINTER_TO_INT(d), &c);
  add_custom_color(GPOINTER_TO_INT(d) == 1 ? &S.c1 : &S.c2);
}

static void act_edit_colors(void) {
  GtkWidget *d = gtk_color_chooser_dialog_new(T("ui.editcolors"), GTK_WINDOW(S.win));
  gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(d), FALSE);
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(d), &S.c1);
  g_object_set(d, "show-editor", TRUE, NULL);
  if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) {
    GdkRGBA c;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(d), &c);
    set_color(1, &c);
    add_custom_color(&S.c1);
  }
  gtk_widget_destroy(d);
}

static gboolean draw_palette(GtkWidget *w, cairo_t *cr, gpointer d) {
  double cell = floor(gtk_widget_get_allocated_height(w) / 3.0);
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 10; c++) {
      cairo_rectangle(cr, c * cell + 2.5, r * cell + 2.5, cell - 5, cell - 5);
      if (r < 2 || c < S.ncustom) {
        GdkRGBA col;
        if (r < 2) gdk_rgba_parse(&col, PALETTE[r][c]); else col = S.custom[c];
        gdk_cairo_set_source_rgba(cr, &col);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.8);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
      } else {
        double dash = 2;
        cairo_set_dash(cr, &dash, 1, 0);
        cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.6);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0);
      }
    }
  return FALSE;
}

static gboolean on_palette_press(GtkWidget *w, GdkEventButton *e, gpointer d) {
  double cell = floor(gtk_widget_get_allocated_height(w) / 3.0);
  int c = (int)(e->x / cell), r = (int)(e->y / cell);
  if (c < 0 || c >= 10 || r < 0 || r > 2) return TRUE;
  GdkRGBA col;
  if (r < 2) gdk_rgba_parse(&col, PALETTE[r][c]);
  else if (c < S.ncustom) col = S.custom[c];
  else return TRUE;
  set_color(e->button == 3 ? 2 : 1, &col);
  return TRUE;
}

/* ---------- asboblar ---------- */

static void apply_tool(Tool t, int sub) {
  if (S.poly_active && !(t == T_SHAPE && sub == SH_POLYGON)) finish_polygon();
  if (S.fl && !((t == T_SELECT || t == T_FREESELECT) && S.fl->kind == O_IMAGE)) commit();
  if (t == T_PICKER && S.tool != T_PICKER) {
    S.prev_tool = S.tool;
    S.prev_sub = S.tool == T_BRUSH ? S.brush : S.shape;
  }
  S.tool = t;
  if (t == T_BRUSH) S.brush = sub;
  if (t == T_SHAPE) S.shape = sub;
  if (S.brush_icon) gtk_widget_queue_draw(S.brush_icon);
  int c = size_cat(t);
  S.updating = TRUE;
  if (c >= 0) gtk_spin_button_set_value(GTK_SPIN_BUTTON(S.size_spin), S.sizes[c]);
  gtk_widget_set_sensitive(S.size_box, c >= 0);
  S.updating = FALSE;
  set_cursor_name(t == T_TEXT ? "text" : t == T_MAGNIFIER ? "zoom-in" : "crosshair");
  update_status();
  redraw();
}

static void on_tool_toggled(GtkToggleButton *b, gpointer d) {
  if (!gtk_toggle_button_get_active(b)) return;
  apply_tool(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "tool")),
             GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "sub")));
}

static void select_tool(Tool t, int sub) {
  GtkWidget *b = t == T_SHAPE ? S.btn_shape[sub] : t == T_BRUSH ? S.brush_btn : S.btn_tool[t];
  if (t == T_BRUSH) g_object_set_data(G_OBJECT(b), "sub", GINT_TO_POINTER(sub));
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b))) apply_tool(t, sub);
  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b), TRUE);
}

static void on_brush_item(GtkMenuItem *it, gpointer d) { select_tool(T_BRUSH, GPOINTER_TO_INT(d)); }

static void on_size_changed(GtkSpinButton *sb, gpointer d) {
  if (S.updating) return;
  double v = gtk_spin_button_get_value(sb);
  int c = size_cat(S.tool);
  if (S.fl && (S.fl->kind == O_SHAPE || S.fl->kind == O_POINTS)) { S.fl->width = v; c = 3; }
  if (c >= 0) S.sizes[c] = v;
  redraw();
}

static void on_style_changed(GtkComboBox *cb, gpointer d) {
  if (S.fl && S.fl->kind == O_SHAPE) {
    S.fl->outline = cur_outline();
    S.fl->filled = cur_filled();
    if (!S.fl->outline && !S.fl->filled) S.fl->outline = TRUE;
    redraw();
  }
}

/* ---------- belgichalar (ikon vidjetlari) ---------- */

static gboolean on_icon_draw(GtkWidget *w, cairo_t *cr, gpointer d) {
  int id = GPOINTER_TO_INT(d);
  if (id == -1) id = IC_BRUSH0 + S.brush;
  int aw = gtk_widget_get_allocated_width(w), ah = gtk_widget_get_allocated_height(w);
  double s = MIN(aw, ah);
  GdkRGBA fg;
  gtk_style_context_get_color(gtk_widget_get_style_context(w), gtk_widget_get_state_flags(w), &fg);
  cairo_translate(cr, (aw - s) / 2, (ah - s) / 2);
  cairo_scale(cr, s / 24.0, s / 24.0);
  draw_icon(cr, id, &fg);
  return FALSE;
}

static GtkWidget *icon_new(int id, int scale_pct) {
  GtkWidget *da = gtk_drawing_area_new();
  g_object_set_data(G_OBJECT(da), "scale", GINT_TO_POINTER(scale_pct));
  g_signal_connect(da, "draw", G_CALLBACK(on_icon_draw), GINT_TO_POINTER(id));
  g_ptr_array_add(S.icons, da);
  return da;
}

static GtkWidget *tool_radio(Tool t, int sub, int icon, const char *tip, int scale) {
  GtkWidget *b = gtk_radio_button_new(tool_group);
  tool_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(b));
  gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(b), FALSE);
  gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
  gtk_style_context_add_class(gtk_widget_get_style_context(b), "uzp-tool");
  gtk_container_add(GTK_CONTAINER(b), icon_new(icon, scale));
  gtk_widget_set_tooltip_text(b, tip);
  g_object_set_data(G_OBJECT(b), "tool", GINT_TO_POINTER(t));
  g_object_set_data(G_OBJECT(b), "sub", GINT_TO_POINTER(sub));
  g_signal_connect(b, "toggled", G_CALLBACK(on_tool_toggled), NULL);
  return b;
}

static GtkWidget *icon_button(int icon, const char *tip, void (*fn)(void)) {
  GtkWidget *b = gtk_button_new();
  gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
  gtk_style_context_add_class(gtk_widget_get_style_context(b), "uzp-tool");
  gtk_container_add(GTK_CONTAINER(b), icon_new(icon, 100));
  gtk_widget_set_tooltip_text(b, tip);
  g_signal_connect_swapped(b, "clicked", G_CALLBACK(fn), NULL);
  return b;
}

static GtkWidget *menu_button(int icon, const char *tip, GtkWidget *menu) {
  GtkWidget *b = gtk_menu_button_new();
  gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
  gtk_style_context_add_class(gtk_widget_get_style_context(b), "uzp-tool");
  GtkWidget *old = gtk_bin_get_child(GTK_BIN(b));
  if (old) gtk_widget_destroy(old);
  gtk_container_add(GTK_CONTAINER(b), icon_new(icon, icon == IC_SELMENU ? 70 : 100));
  gtk_widget_set_tooltip_text(b, tip);
  gtk_widget_show_all(menu);
  gtk_menu_button_set_popup(GTK_MENU_BUTTON(b), menu);
  return b;
}

/* ---------- menyular ---------- */

static GtkWidget *mitem(GtkWidget *menu, const char *key, const char *accel, void (*fn)(void)) {
  GtkWidget *it = gtk_menu_item_new_with_label(T(key));
  if (accel) {
    guint k;
    GdkModifierType m;
    gtk_accelerator_parse(accel, &k, &m);
    if (k) gtk_widget_add_accelerator(it, "activate", S.accel, k, m, GTK_ACCEL_VISIBLE);
  }
  if (fn) g_signal_connect_swapped(it, "activate", G_CALLBACK(fn), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
  return it;
}

/* faqat ko'rsatish uchun tugma yozuvi (tugmani o'zimiz ushlaymiz) */
static void mitem_hint(GtkWidget *it, const char *accel) {
  guint k;
  GdkModifierType m;
  gtk_accelerator_parse(accel, &k, &m);
  GtkWidget *child = gtk_bin_get_child(GTK_BIN(it));
  if (GTK_IS_ACCEL_LABEL(child)) gtk_accel_label_set_accel(GTK_ACCEL_LABEL(child), k, m);
}

static void add_accel(GtkWidget *it, const char *accel) {
  guint k;
  GdkModifierType m;
  gtk_accelerator_parse(accel, &k, &m);
  if (k) gtk_widget_add_accelerator(it, "activate", S.accel, k, m, 0);
}

static GtkWidget *check_item(GtkWidget *menu, const char *key, const char *accel, gboolean active, GCallback cb) {
  GtkWidget *it = gtk_check_menu_item_new_with_label(T(key));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it), active);
  if (accel) {
    guint k;
    GdkModifierType m;
    gtk_accelerator_parse(accel, &k, &m);
    if (k) gtk_widget_add_accelerator(it, "activate", S.accel, k, m, GTK_ACCEL_VISIBLE);
  }
  g_signal_connect(it, "toggled", cb, NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
  return it;
}

static GtkWidget *submenu(GtkWidget *bar, const char *key) {
  GtkWidget *top = gtk_menu_item_new_with_label(T(key));
  GtkWidget *m = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(m), S.accel);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(top), m);
  gtk_menu_shell_append(GTK_MENU_SHELL(bar), top);
  return m;
}

static void sep(GtkWidget *menu) { gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new()); }

static void apply_view(void) {
  gtk_widget_set_visible(S.hruler, S.rulers);
  gtk_widget_set_visible(S.vruler, S.rulers);
  gtk_widget_set_visible(S.corner, S.rulers);
  gtk_widget_set_visible(S.statusbar, S.status_on);
  gtk_widget_set_visible(S.thumb_frame, S.thumb_on);
  redraw();
}

static void on_view_toggle(GtkCheckMenuItem *it, gpointer d) {
  gboolean a = gtk_check_menu_item_get_active(it);
  GtkWidget *w = GTK_WIDGET(it);
  if (w == S.mi_rulers) S.rulers = a;
  else if (w == S.mi_grid) S.grid_on = a;
  else if (w == S.mi_status) S.status_on = a;
  else if (w == S.mi_thumb) S.thumb_on = a;
  apply_view();
  save_settings();
}

static void on_transparent(GtkCheckMenuItem *it, gpointer d) {
  S.transparent_sel = gtk_check_menu_item_get_active(it);
  redraw();
}

static void act_undo(void) { do_undo(); }
static void act_redo(void) { do_redo(); }
static void act_save(void) { do_save(); }
static void act_save_as(void) { do_save_as(); }
static void act_select_all(void) { select_all(); }
static void act_delete(void) { delete_selection(); }
static void act_zoom_in(void) { zoom_step(1, -1, -1); }
static void act_zoom_out(void) { zoom_step(-1, -1, -1); }
static void act_zoom_fit(void) { zoom_fit(); }
static void act_zoom_100(void) { set_zoom(1, -1, -1); }

static void act_exit(void) {
  if (!confirm_discard()) return;
  S.dirty = FALSE;
  gtk_widget_destroy(S.win);
}

static void act_fullscreen(void) {
  if (S.fullscreen) gtk_window_unfullscreen(GTK_WINDOW(S.win));
  else gtk_window_fullscreen(GTK_WINDOW(S.win));
}

static gboolean on_window_state(GtkWidget *w, GdkEventWindowState *e, gpointer d) {
  S.fullscreen = (e->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
  return FALSE;
}

static void act_keys(void) {
  GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(S.win), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                                        GTK_BUTTONS_CLOSE, "%s", T("help.keys"));
  gtk_button_set_label(GTK_BUTTON(gtk_dialog_get_widget_for_response(GTK_DIALOG(d), GTK_RESPONSE_CLOSE)), T("ui.close"));
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", T("help.keystext"));
  gtk_dialog_run(GTK_DIALOG(d));
  gtk_widget_destroy(d);
}

static void act_about(void) {
  GdkPixbuf *logo = make_app_icon(128);
  const char *authors[] = { "Uz Paint mualliflari", NULL };
  gtk_show_about_dialog(GTK_WINDOW(S.win),
      "program-name", APP_NAME,
      "version", APP_VERSION,
      "comments", T("help.comment"),
      "copyright", "© 2026 Uz Paint mualliflari",
      "license-type", GTK_LICENSE_MIT_X11,
      "authors", authors,
      "logo", logo,
      NULL);
  g_object_unref(logo);
}

static GtkWidget *build_menubar(void) {
  GtkWidget *bar = gtk_menu_bar_new();
  GtkWidget *m, *it;

  m = submenu(bar, "menu.file");
  mitem(m, "act.new", "<Control>n", act_new);
  mitem(m, "act.open", "<Control>o", act_open);
  it = mitem(m, "act.recent", NULL, NULL);
  S.recent_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(it), S.recent_menu);
  rebuild_recent_menu();
  sep(m);
  mitem(m, "act.save", "<Control>s", act_save);
  mitem(m, "act.saveas", "<Control><Shift>s", act_save_as);
  sep(m);
  mitem(m, "act.print", "<Control>p", act_print);
  mitem(m, "act.wallpaper", NULL, act_wallpaper);
  mitem(m, "act.properties", "<Control>e", act_properties);
  sep(m);
  mitem(m, "act.exit", "<Control>q", act_exit);

  m = submenu(bar, "menu.edit");
  mitem(m, "act.undo", "<Control>z", act_undo);
  it = mitem(m, "act.redo", "<Control>y", act_redo);
  add_accel(it, "<Control><Shift>z");
  sep(m);
  mitem(m, "act.cut", "<Control>x", act_cut);
  mitem(m, "act.copy", "<Control>c", act_copy);
  mitem(m, "act.paste", "<Control>v", act_paste);
  mitem(m, "act.pastefrom", NULL, act_paste_from);
  sep(m);
  mitem(m, "act.selectall", "<Control>a", act_select_all);
  it = mitem(m, "act.delete", NULL, act_delete);
  mitem_hint(it, "Delete");

  m = submenu(bar, "menu.view");
  it = mitem(m, "act.zoomin", "<Control>equal", act_zoom_in);
  add_accel(it, "<Control>plus");
  add_accel(it, "<Control>KP_Add");
  it = mitem(m, "act.zoomout", "<Control>minus", act_zoom_out);
  add_accel(it, "<Control>KP_Subtract");
  mitem(m, "act.fit", "<Control>0", act_zoom_fit);
  mitem(m, "act.actual", "<Control>1", act_zoom_100);
  sep(m);
  S.mi_rulers = check_item(m, "act.rulers", "<Control>r", S.rulers, G_CALLBACK(on_view_toggle));
  S.mi_grid = check_item(m, "act.grid", "<Control>g", S.grid_on, G_CALLBACK(on_view_toggle));
  S.mi_status = check_item(m, "act.status", NULL, S.status_on, G_CALLBACK(on_view_toggle));
  S.mi_thumb = check_item(m, "act.thumb", NULL, S.thumb_on, G_CALLBACK(on_view_toggle));
  sep(m);
  mitem(m, "act.fullscreen", "F11", act_fullscreen);

  m = submenu(bar, "menu.image");
  mitem(m, "act.crop", "<Control><Shift>x", act_crop);
  mitem(m, "act.resize", "<Control>w", act_resize);
  sep(m);
  mitem(m, "act.rotright", NULL, act_rot_right);
  mitem(m, "act.rotleft", NULL, act_rot_left);
  mitem(m, "act.rot180", NULL, act_rot_180);
  mitem(m, "act.fliph", NULL, act_flip_h);
  mitem(m, "act.flipv", NULL, act_flip_v);
  sep(m);
  mitem(m, "act.invert", "<Control><Shift>i", act_invert);
  mitem(m, "act.clear", NULL, act_clear);

  m = submenu(bar, "menu.settings");
  it = mitem(m, "set.theme", NULL, NULL);
  GtkWidget *tm = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(it), tm);
  GSList *grp = NULL;
  const char *tk[3] = { "set.system", "set.light", "set.dark" };
  S.updating = TRUE;
  for (int i = 0; i < 3; i++) {
    S.mi_theme[i] = gtk_radio_menu_item_new_with_label(grp, T(tk[i]));
    grp = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(S.mi_theme[i]));
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(S.mi_theme[i]), S.theme == i);
    g_signal_connect(S.mi_theme[i], "toggled", G_CALLBACK(on_theme), GINT_TO_POINTER(i));
    gtk_menu_shell_append(GTK_MENU_SHELL(tm), S.mi_theme[i]);
  }
  it = mitem(m, "set.lang", NULL, NULL);
  GtkWidget *lm = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(it), lm);
  grp = NULL;
  const char *ln[2] = { "Özbekça", "English" };
  for (int i = 0; i < 2; i++) {
    S.mi_lang[i] = gtk_radio_menu_item_new_with_label(grp, ln[i]);
    grp = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(S.mi_lang[i]));
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(S.mi_lang[i]), S.lang == i);
    g_signal_connect(S.mi_lang[i], "toggled", G_CALLBACK(on_lang), GINT_TO_POINTER(i));
    gtk_menu_shell_append(GTK_MENU_SHELL(lm), S.mi_lang[i]);
  }
  S.updating = FALSE;
  mitem(m, "set.font", NULL, act_ui_font);

  m = submenu(bar, "menu.help");
  mitem(m, "help.keys", "F1", act_keys);
  mitem(m, "help.about", NULL, act_about);
  return bar;
}

/* ---------- lenta (ribbon) ---------- */

static void ribbon_group(const char *key, GtkWidget *content, gboolean last) {
  GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_style_context_add_class(gtk_widget_get_style_context(v), last ? "uzp-group-last" : "uzp-group");
  gtk_widget_set_valign(content, GTK_ALIGN_CENTER);
  gtk_widget_set_halign(content, GTK_ALIGN_CENTER);
  gtk_box_pack_start(GTK_BOX(v), content, TRUE, TRUE, 0);
  GtkWidget *l = gtk_label_new(T(key));
  gtk_style_context_add_class(gtk_widget_get_style_context(l), "dim-label");
  gtk_box_pack_end(GTK_BOX(v), l, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(S.ribbon), v, FALSE, FALSE, 0);
}

/* lenta sig'masa, sichqoncha g'ildiragi bilan yonga suriladi */
static gboolean on_ribbon_scroll(GtkWidget *w, GdkEventScroll *e, gpointer d) {
  GtkAdjustment *a = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(w));
  double dy = 0;
  if (e->direction == GDK_SCROLL_UP || e->direction == GDK_SCROLL_LEFT) dy = -1;
  else if (e->direction == GDK_SCROLL_DOWN || e->direction == GDK_SCROLL_RIGHT) dy = 1;
  else if (e->direction == GDK_SCROLL_SMOOTH) dy = fabs(e->delta_x) > fabs(e->delta_y) ? e->delta_x : e->delta_y;
  gtk_adjustment_set_value(a, gtk_adjustment_get_value(a) + dy * 60);
  return TRUE;
}

static GtkWidget *color_slot(GtkWidget *btn, const char *cap) {
  GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_box_pack_start(GTK_BOX(v), btn, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(v), gtk_label_new(cap), FALSE, FALSE, 0);
  return v;
}

static void build_ribbon(void) {
  S.ribbon = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_set_border_width(GTK_CONTAINER(S.ribbon), 4);

  /* tarix */
  GtkWidget *g = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(g), icon_button(IC_UNDO, T("act.undo"), act_undo), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(g), icon_button(IC_REDO, T("act.redo"), act_redo), FALSE, FALSE, 0);
  ribbon_group("grp.history", g, FALSE);

  /* tanlash */
  g = gtk_grid_new();
  S.btn_tool[T_SELECT] = tool_radio(T_SELECT, 0, IC_SELECT, T("tool.select"), 130);
  S.btn_tool[T_FREESELECT] = tool_radio(T_FREESELECT, 0, IC_FREESELECT, T("tool.freeselect"), 100);
  GtkWidget *sm = gtk_menu_new();
  GtkWidget *it = gtk_menu_item_new_with_label(T("act.selectall"));
  g_signal_connect_swapped(it, "activate", G_CALLBACK(act_select_all), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(sm), it);
  S.mi_transparent = gtk_check_menu_item_new_with_label(T("act.transparent"));
  g_signal_connect(S.mi_transparent, "toggled", G_CALLBACK(on_transparent), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(sm), S.mi_transparent);
  it = gtk_menu_item_new_with_label(T("act.delete"));
  g_signal_connect_swapped(it, "activate", G_CALLBACK(act_delete), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(sm), it);
  it = gtk_menu_item_new_with_label(T("act.crop"));
  g_signal_connect_swapped(it, "activate", G_CALLBACK(act_crop), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(sm), it);
  gtk_grid_attach(GTK_GRID(g), S.btn_tool[T_SELECT], 0, 0, 1, 2);
  gtk_grid_attach(GTK_GRID(g), S.btn_tool[T_FREESELECT], 1, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(g), menu_button(IC_SELMENU, T("tool.selmenu"), sm), 1, 1, 1, 1);
  ribbon_group("grp.select", g, FALSE);

  /* rasm */
  g = gtk_grid_new();
  GtkWidget *rm = gtk_menu_new();
  const char *rk[5] = { "act.rotright", "act.rotleft", "act.rot180", "act.fliph", "act.flipv" };
  void (*rf[5])(void) = { act_rot_right, act_rot_left, act_rot_180, act_flip_h, act_flip_v };
  for (int i = 0; i < 5; i++) {
    it = gtk_menu_item_new_with_label(T(rk[i]));
    g_signal_connect_swapped(it, "activate", G_CALLBACK(rf[i]), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(rm), it);
  }
  gtk_grid_attach(GTK_GRID(g), icon_button(IC_CROP, T("act.crop"), act_crop), 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(g), icon_button(IC_RESIZE, T("act.resize"), act_resize), 1, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(g), menu_button(IC_ROTATE, T("act.rotate"), rm), 0, 1, 2, 1);
  ribbon_group("grp.image", g, FALSE);

  /* asboblar */
  g = gtk_grid_new();
  struct { Tool t; int ic; const char *k; } tl[6] = {
    { T_PENCIL, IC_PENCIL, "tool.pencil" }, { T_FILL, IC_FILL, "tool.fill" }, { T_TEXT, IC_TEXT, "tool.text" },
    { T_ERASER, IC_ERASER, "tool.eraser" }, { T_PICKER, IC_PICKER, "tool.picker" }, { T_MAGNIFIER, IC_MAGNIFIER, "tool.magnifier" } };
  for (int i = 0; i < 6; i++) {
    S.btn_tool[tl[i].t] = tool_radio(tl[i].t, 0, tl[i].ic, T(tl[i].k), 100);
    gtk_grid_attach(GTK_GRID(g), S.btn_tool[tl[i].t], i % 3, i / 3, 1, 1);
  }
  ribbon_group("grp.tools", g, FALSE);

  /* mo'yqalamlar */
  g = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  S.brush_btn = tool_radio(T_BRUSH, 0, -1, T("brush.0"), 160);
  S.btn_tool[T_BRUSH] = S.brush_btn;
  S.brush_icon = gtk_bin_get_child(GTK_BIN(S.brush_btn));
  GtkWidget *bm = gtk_menu_new();
  for (int b = 0; b < B_COUNT; b++) {
    GtkWidget *mi = gtk_menu_item_new();
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(hb), icon_new(IC_BRUSH0 + b, 120), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hb), gtk_label_new(Tn("brush", b)), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(mi), hb);
    g_signal_connect(mi, "activate", G_CALLBACK(on_brush_item), GINT_TO_POINTER(b));
    gtk_menu_shell_append(GTK_MENU_SHELL(bm), mi);
  }
  gtk_box_pack_start(GTK_BOX(g), S.brush_btn, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(g), menu_button(IC_SELMENU, T("tool.brushmenu"), bm), FALSE, FALSE, 0);
  ribbon_group("grp.brushes", g, FALSE);

  /* shakllar */
  GtkWidget *sh = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  g = gtk_grid_new();
  for (int s = 0; s < SH_COUNT; s++) {
    S.btn_shape[s] = tool_radio(T_SHAPE, s, IC_SHAPE0 + s, Tn("shape", s), 85);
    gtk_grid_attach(GTK_GRID(g), S.btn_shape[s], s % 8, s / 8, 1, 1);
  }
  gtk_box_pack_start(GTK_BOX(sh), g, FALSE, FALSE, 0);
  GtkWidget *og = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(og), 4);
  gtk_grid_set_column_spacing(GTK_GRID(og), 6);
  gtk_widget_set_valign(og, GTK_ALIGN_CENTER);
  S.outline_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(S.outline_combo), T("ui.none"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(S.outline_combo), T("ui.solid1"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(S.outline_combo), 1);
  S.fill_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(S.fill_combo), T("ui.none"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(S.fill_combo), T("ui.solid2"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(S.fill_combo), 0);
  g_signal_connect(S.outline_combo, "changed", G_CALLBACK(on_style_changed), NULL);
  g_signal_connect(S.fill_combo, "changed", G_CALLBACK(on_style_changed), NULL);
  gtk_grid_attach(GTK_GRID(og), grid_label(T("ui.outline")), 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(og), S.outline_combo, 1, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(og), grid_label(T("ui.fill")), 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(og), S.fill_combo, 1, 1, 1, 1);
  gtk_box_pack_start(GTK_BOX(sh), og, FALSE, FALSE, 0);
  ribbon_group("grp.shapes", sh, FALSE);

  /* qalinlik */
  S.size_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  S.size_spin = gtk_spin_button_new_with_range(1, 200, 1);
  gtk_orientable_set_orientation(GTK_ORIENTABLE(S.size_spin), GTK_ORIENTATION_VERTICAL);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(S.size_spin), 1);
  gtk_entry_set_width_chars(GTK_ENTRY(S.size_spin), 3);
  g_signal_connect(S.size_spin, "value-changed", G_CALLBACK(on_size_changed), NULL);
  gtk_box_pack_start(GTK_BOX(S.size_box), S.size_spin, FALSE, FALSE, 0);
  ribbon_group("grp.size", S.size_box, FALSE);

  /* ranglar */
  GtkWidget *cb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  S.cbtn1 = gtk_color_button_new_with_rgba(&S.c1);
  S.cbtn2 = gtk_color_button_new_with_rgba(&S.c2);
  gtk_widget_set_tooltip_text(S.cbtn1, T("ui.color1"));
  gtk_widget_set_tooltip_text(S.cbtn2, T("ui.color2"));
  g_signal_connect(S.cbtn1, "color-set", G_CALLBACK(on_color_set), GINT_TO_POINTER(1));
  g_signal_connect(S.cbtn2, "color-set", G_CALLBACK(on_color_set), GINT_TO_POINTER(2));
  gtk_box_pack_start(GTK_BOX(cb), color_slot(S.cbtn1, T("ui.solid1")), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(cb), color_slot(S.cbtn2, T("ui.solid2")), FALSE, FALSE, 0);
  S.palette = gtk_drawing_area_new();
  gtk_widget_set_valign(S.palette, GTK_ALIGN_CENTER);
  gtk_widget_set_tooltip_text(S.palette, T("ui.palette"));
  gtk_widget_add_events(S.palette, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(S.palette, "draw", G_CALLBACK(draw_palette), NULL);
  g_signal_connect(S.palette, "button-press-event", G_CALLBACK(on_palette_press), NULL);
  gtk_box_pack_start(GTK_BOX(cb), S.palette, FALSE, FALSE, 0);
  GtkWidget *eb = gtk_button_new_with_label("+");
  gtk_widget_set_tooltip_text(eb, T("ui.editcolors"));
  gtk_widget_set_valign(eb, GTK_ALIGN_CENTER);
  g_signal_connect_swapped(eb, "clicked", G_CALLBACK(act_edit_colors), NULL);
  gtk_box_pack_start(GTK_BOX(cb), eb, FALSE, FALSE, 0);
  ribbon_group("grp.colors", cb, TRUE);
}

/* ---------- klaviatura ---------- */

static gboolean on_key(GtkWidget *w, GdkEventKey *e, gpointer d) {
  GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(S.win));
  if (focus && GTK_IS_EDITABLE(focus)) {
    if (gtk_window_propagate_key_event(GTK_WINDOW(S.win), e)) return TRUE;
    return FALSE;
  }
  if (e->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) return FALSE;
  double step = (e->state & GDK_SHIFT_MASK) ? 10 : 1;
  switch (e->keyval) {
  case GDK_KEY_Escape: case GDK_KEY_Return: case GDK_KEY_KP_Enter:
    if (S.poly_active) { finish_polygon(); return TRUE; }
    if (S.fl) { commit(); return TRUE; }
    if (e->keyval == GDK_KEY_Escape && S.fullscreen) { gtk_window_unfullscreen(GTK_WINDOW(S.win)); return TRUE; }
    return FALSE;
  case GDK_KEY_Delete: case GDK_KEY_KP_Delete:
    if (S.fl) { delete_selection(); return TRUE; }
    return FALSE;
  case GDK_KEY_Left: case GDK_KEY_Right: case GDK_KEY_Up: case GDK_KEY_Down:
    if (!S.fl) return FALSE;
    {
      double dx = e->keyval == GDK_KEY_Left ? -step : e->keyval == GDK_KEY_Right ? step : 0;
      double dy = e->keyval == GDK_KEY_Up ? -step : e->keyval == GDK_KEY_Down ? step : 0;
      if (S.fl->kind == O_POINTS)
        for (int i = 0; i < S.fl->npts; i++) { S.fl->pts[2 * i] += dx; S.fl->pts[2 * i + 1] += dy; }
      else { S.fl->x += dx; S.fl->y += dy; }
      redraw();
    }
    return TRUE;
  }
  return FALSE;
}

static gboolean on_delete_event(GtkWidget *w, GdkEvent *e, gpointer d) {
  if (!confirm_discard()) return TRUE;
  return FALSE;
}

static void on_destroy(GtkWidget *w, gpointer d) {
  save_settings();
  gtk_main_quit();
}

static gboolean on_configure(GtkWidget *w, GdkEventConfigure *e, gpointer d) {
  if (!S.fullscreen && !gtk_window_is_maximized(GTK_WINDOW(w)))
    gtk_window_get_size(GTK_WINDOW(w), &S.win_w, &S.win_h);
  return FALSE;
}

static void on_drop(GtkWidget *w, GdkDragContext *ctx, int x, int y, GtkSelectionData *sd, guint info, guint t, gpointer d) {
  char **uris = gtk_selection_data_get_uris(sd);
  gboolean ok = FALSE;
  if (uris && uris[0]) {
    char *fn = g_filename_from_uri(uris[0], NULL, NULL);
    if (fn && confirm_discard()) ok = load_file(fn);
    g_free(fn);
  }
  g_strfreev(uris);
  gtk_drag_finish(ctx, ok, FALSE, t);
}

static void on_zoom_scale(GtkRange *r, gpointer d) {
  if (S.updating) return;
  set_zoom(pow(2, gtk_range_get_value(r)), -1, -1);
}

static gboolean draw_corner(GtkWidget *w, cairo_t *cr, gpointer d) {
  gtk_render_background(gtk_widget_get_style_context(w), cr, 0, 0,
                        gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w));
  return FALSE;
}

static GtkWidget *small_button(const char *label, const char *tip, void (*fn)(void)) {
  GtkWidget *b = gtk_button_new_with_label(label);
  gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
  gtk_widget_set_tooltip_text(b, tip);
  g_signal_connect_swapped(b, "clicked", G_CALLBACK(fn), NULL);
  return b;
}

static void build_ui(void) {
  S.win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size(GTK_WINDOW(S.win), S.win_w, S.win_h);
  GdkPixbuf *ic = make_app_icon(128);
  gtk_window_set_icon(GTK_WINDOW(S.win), ic);
  g_object_unref(ic);
  S.accel = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(S.win), S.accel);
  g_signal_connect(S.win, "delete-event", G_CALLBACK(on_delete_event), NULL);
  g_signal_connect(S.win, "destroy", G_CALLBACK(on_destroy), NULL);
  g_signal_connect(S.win, "key-press-event", G_CALLBACK(on_key), NULL);
  g_signal_connect(S.win, "window-state-event", G_CALLBACK(on_window_state), NULL);
  g_signal_connect(S.win, "configure-event", G_CALLBACK(on_configure), NULL);

  S.vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(S.win), S.vbox);
  S.menubar = build_menubar();
  gtk_box_pack_start(GTK_BOX(S.vbox), S.menubar, FALSE, FALSE, 0);

  build_ribbon();
  S.ribbon_scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(S.ribbon_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
  gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(S.ribbon_scroll), FALSE);
  gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(S.ribbon_scroll), TRUE);
  gtk_container_add(GTK_CONTAINER(S.ribbon_scroll), S.ribbon);
  gtk_widget_add_events(S.ribbon_scroll, GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK);
  g_signal_connect(S.ribbon_scroll, "scroll-event", G_CALLBACK(on_ribbon_scroll), NULL);
  gtk_box_pack_start(GTK_BOX(S.vbox), S.ribbon_scroll, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(S.vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

  /* kanvas, chizg'ichlar, kichik oyna */
  S.grid = gtk_grid_new();
  S.corner = gtk_drawing_area_new();
  S.hruler = gtk_drawing_area_new();
  S.vruler = gtk_drawing_area_new();
  GtkWidget *rl[3] = { S.corner, S.hruler, S.vruler };
  for (int i = 0; i < 3; i++) {
    gtk_style_context_add_class(gtk_widget_get_style_context(rl[i]), "uzp-ruler");
    gtk_widget_set_no_show_all(rl[i], TRUE);
  }
  g_signal_connect(S.corner, "draw", G_CALLBACK(draw_corner), NULL);
  g_signal_connect(S.hruler, "draw", G_CALLBACK(draw_ruler), GINT_TO_POINTER(1));
  g_signal_connect(S.vruler, "draw", G_CALLBACK(draw_ruler), GINT_TO_POINTER(0));
  gtk_widget_set_hexpand(S.hruler, TRUE);
  gtk_widget_set_vexpand(S.vruler, TRUE);
  gtk_grid_attach(GTK_GRID(S.grid), S.corner, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(S.grid), S.hruler, 1, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(S.grid), S.vruler, 0, 1, 1, 1);

  S.overlay = gtk_overlay_new();
  S.scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_style_context_add_class(gtk_widget_get_style_context(S.scroll), "uzp-canvasbg");
  gtk_widget_set_hexpand(S.scroll, TRUE);
  gtk_widget_set_vexpand(S.scroll, TRUE);
  S.area = gtk_drawing_area_new();
  gtk_widget_set_halign(S.area, GTK_ALIGN_START);
  gtk_widget_set_valign(S.area, GTK_ALIGN_START);
  gtk_widget_set_can_focus(S.area, TRUE);
  gtk_widget_add_events(S.area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
                        GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_LEAVE_NOTIFY_MASK);
  g_signal_connect(S.area, "draw", G_CALLBACK(on_draw), NULL);
  g_signal_connect(S.area, "button-press-event", G_CALLBACK(on_press), NULL);
  g_signal_connect(S.area, "button-release-event", G_CALLBACK(on_release), NULL);
  g_signal_connect(S.area, "motion-notify-event", G_CALLBACK(on_motion), NULL);
  g_signal_connect(S.area, "scroll-event", G_CALLBACK(on_scroll), NULL);
  g_signal_connect(S.area, "leave-notify-event", G_CALLBACK(on_leave), NULL);
  g_signal_connect_after(S.area, "realize", G_CALLBACK(on_area_realize), NULL);
  g_signal_connect_after(S.area, "size-allocate", G_CALLBACK(on_area_allocate), NULL);
  gtk_container_add(GTK_CONTAINER(S.scroll), S.area);
  GtkWidget *vp = gtk_bin_get_child(GTK_BIN(S.scroll));
  if (vp) gtk_style_context_add_class(gtk_widget_get_style_context(vp), "uzp-canvasbg");
  g_signal_connect(hadj(), "value-changed", G_CALLBACK(on_adj_changed), NULL);
  g_signal_connect(vadj(), "value-changed", G_CALLBACK(on_adj_changed), NULL);
  g_signal_connect(hadj(), "changed", G_CALLBACK(on_adj_changed), NULL);
  g_signal_connect(vadj(), "changed", G_CALLBACK(on_adj_changed), NULL);
  gtk_container_add(GTK_CONTAINER(S.overlay), S.scroll);

  S.thumb_frame = gtk_frame_new(NULL);
  gtk_style_context_add_class(gtk_widget_get_style_context(S.thumb_frame), "uzp-thumb");
  gtk_widget_set_halign(S.thumb_frame, GTK_ALIGN_END);
  gtk_widget_set_valign(S.thumb_frame, GTK_ALIGN_START);
  gtk_widget_set_margin_end(S.thumb_frame, 18);
  gtk_widget_set_margin_top(S.thumb_frame, 12);
  S.thumb = gtk_drawing_area_new();
  gtk_widget_add_events(S.thumb, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  g_signal_connect(S.thumb, "draw", G_CALLBACK(draw_thumb), NULL);
  g_signal_connect(S.thumb, "button-press-event", G_CALLBACK(on_thumb_press), NULL);
  g_signal_connect(S.thumb, "motion-notify-event", G_CALLBACK(on_thumb_motion), NULL);
  g_signal_connect(S.thumb, "button-release-event", G_CALLBACK(on_thumb_release), NULL);
  gtk_container_add(GTK_CONTAINER(S.thumb_frame), S.thumb);
  gtk_widget_show(S.thumb);
  gtk_widget_set_no_show_all(S.thumb_frame, TRUE);
  gtk_overlay_add_overlay(GTK_OVERLAY(S.overlay), S.thumb_frame);
  gtk_grid_attach(GTK_GRID(S.grid), S.overlay, 1, 1, 1, 1);
  gtk_box_pack_start(GTK_BOX(S.vbox), S.grid, TRUE, TRUE, 0);

  gtk_drag_dest_set(S.area, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_uri_targets(S.area);
  g_signal_connect(S.area, "drag-data-received", G_CALLBACK(on_drop), NULL);

  /* holat paneli */
  S.statusbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_widget_set_no_show_all(S.statusbar, TRUE);
  gtk_container_set_border_width(GTK_CONTAINER(S.statusbar), 4);
  gtk_widget_set_margin_start(S.statusbar, 8);
  S.st_pos = gtk_label_new("");
  S.st_sel = gtk_label_new("");
  S.st_size = gtk_label_new("");
  S.st_file = gtk_label_new("");
  gtk_label_set_width_chars(GTK_LABEL(S.st_pos), 14);
  gtk_label_set_xalign(GTK_LABEL(S.st_pos), 0);
  GtkWidget *items[4] = { S.st_pos, S.st_sel, S.st_size, S.st_file };
  for (int i = 0; i < 4; i++) { gtk_box_pack_start(GTK_BOX(S.statusbar), items[i], FALSE, FALSE, 0); gtk_widget_show(items[i]); }
  GtkWidget *spacer = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(S.statusbar), spacer, TRUE, TRUE, 0);
  gtk_widget_show(spacer);
  GtkWidget *zo = small_button("−", T("act.zoomout"), act_zoom_out);
  S.zoom_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -3, 3, 0.05);
  gtk_scale_set_draw_value(GTK_SCALE(S.zoom_scale), FALSE);
  gtk_widget_set_size_request(S.zoom_scale, 150, -1);
  gtk_range_set_value(GTK_RANGE(S.zoom_scale), 0);
  g_signal_connect(S.zoom_scale, "value-changed", G_CALLBACK(on_zoom_scale), NULL);
  GtkWidget *zi = small_button("+", T("act.zoomin"), act_zoom_in);
  S.zoom_label = gtk_label_new("100%");
  gtk_label_set_width_chars(GTK_LABEL(S.zoom_label), 6);
  GtkWidget *zitems[4] = { zo, S.zoom_scale, zi, S.zoom_label };
  for (int i = 0; i < 4; i++) { gtk_box_pack_start(GTK_BOX(S.statusbar), zitems[i], FALSE, FALSE, 0); gtk_widget_show(zitems[i]); }
  gtk_box_pack_start(GTK_BOX(S.vbox), S.statusbar, FALSE, FALSE, 0);
}

/* ---------- ishga tushirish ---------- */

static gboolean open_initial(gpointer fn) {
  load_file(fn);
  g_free(fn);
  return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
  if (argc == 3 && !strcmp(argv[1], "--export-icon")) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 256, 256);
    cairo_t *cr = cairo_create(s);
    draw_app_icon(cr, 256);
    cairo_destroy(cr);
    int r = cairo_surface_write_to_png(s, argv[2]) == CAIRO_STATUS_SUCCESS ? 0 : 1;
    cairo_surface_destroy(s);
    return r;
  }
  if (argc == 2 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v"))) {
    printf("%s %s\n", APP_NAME, APP_VERSION);
    return 0;
  }
  if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
    printf("%s %s\n\nIşlatiş / Usage:\n  uz-paint [RASM_FAYLI]\n  uz-paint --version\n", APP_NAME, APP_VERSION);
    return 0;
  }
  g_set_prgname(APP_ID);
  g_set_application_name(APP_NAME);
  gtk_init(&argc, &argv);
  gtk_window_set_default_icon_name(APP_ID);

  memset(&S, 0, sizeof S);
  S.zoom = 1;
  S.tool = S.prev_tool = T_PENCIL;
  S.sizes[0] = 1; S.sizes[1] = 6; S.sizes[2] = 12; S.sizes[3] = 3;
  gdk_rgba_parse(&S.c1, "#000000");
  gdk_rgba_parse(&S.c2, "#ffffff");
  S.path = g_array_new(FALSE, FALSE, sizeof(double));
  S.icons = g_ptr_array_new();
  load_settings();

  GtkSettings *gs = gtk_settings_get_default();
  g_object_get(gs, "gtk-theme-name", &S.orig_theme, "gtk-application-prefer-dark-theme", &orig_prefer_dark, NULL);
  detect_system_theme();
  apply_theme();

  S.css = gtk_css_provider_new();
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(S.css),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  css_colors = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css_colors,
      ".uzp-canvasbg, .uzp-canvasbg viewport { background-color: mix(@theme_bg_color, @theme_fg_color, 0.12); }\n"
      ".uzp-ruler { background-color: @theme_base_color; }\n"
      ".uzp-thumb { background-color: @theme_base_color; }\n"
      ".uzp-tool { padding: 3px; min-height: 0; min-width: 0; }\n"
      ".uzp-group, .uzp-group-last { padding: 0 6px; }\n"
      ".uzp-group { border-right: 1px solid alpha(@theme_fg_color, 0.15); }\n", -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css_colors),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  build_ui();
  apply_ui_font();
  reset_doc(surf_new(1000, 700), NULL);
  gtk_widget_show_all(S.win);
  apply_view();
  select_tool(T_PENCIL, 0);
  gtk_widget_grab_focus(S.area);
  if (argc > 1 && argv[1][0] != '-') g_idle_add(open_initial, g_strdup(argv[1]));
  gtk_main();
  return 0;
}
