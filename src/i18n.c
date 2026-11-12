/*
 * Uz Paint — tarjimalar.
 * O'zbekcha matnlar yangi lotin alifbosida (ş, ç, ö, ğ, ʼ).
 * Copyright (c) 2026 Uz Paint mualliflari — MIT litsenziyasi.
 */

typedef struct { const char *k, *uz, *en; } Str;

static const Str STRS[] = {
  /* menyular */
  { "menu.file",       "Fayl",            "File" },
  { "menu.edit",       "Tahrir",          "Edit" },
  { "menu.view",       "Köriniş",         "View" },
  { "menu.image",      "Rasm",            "Image" },
  { "menu.settings",   "Sozlamalar",      "Settings" },
  { "menu.help",       "Yordam",          "Help" },

  /* fayl */
  { "act.new",         "Yangi",                        "New" },
  { "act.open",        "Oçiş…",                        "Open…" },
  { "act.recent",      "Oxirgi fayllar",               "Recent files" },
  { "act.norecent",    "(hozirça böş)",                "(empty)" },
  { "act.save",        "Saqlaş",                       "Save" },
  { "act.saveas",      "Boşqa nom bilan saqlaş…",      "Save as…" },
  { "act.print",       "Çop etiş…",                    "Print…" },
  { "act.wallpaper",   "Iş stoli foni qiliş",          "Set as desktop background" },
  { "act.properties",  "Rasm xususiyatlari…",          "Image properties…" },
  { "act.exit",        "Çiqiş",                        "Exit" },

  /* tahrir */
  { "act.undo",        "Orqaga",                       "Undo" },
  { "act.redo",        "Oldinga",                      "Redo" },
  { "act.cut",         "Kesib oliş",                   "Cut" },
  { "act.copy",        "Nusxa oliş",                   "Copy" },
  { "act.paste",       "Qöyiş",                        "Paste" },
  { "act.pastefrom",   "Fayldan qöyiş…",               "Paste from file…" },
  { "act.selectall",   "Hammasini tanlaş",             "Select all" },
  { "act.delete",      "Tanlanganni öçiriş",           "Delete selection" },
  { "act.transparent", "Şaffof tanlaş",                "Transparent selection" },

  /* ko'rinish */
  { "act.zoomin",      "Kattalaştiriş",                "Zoom in" },
  { "act.zoomout",     "Kiçraytiriş",                  "Zoom out" },
  { "act.fit",         "Oynaga moslaş",                "Fit to window" },
  { "act.actual",      "Asl ölçam (100%)",             "Actual size (100%)" },
  { "act.rulers",      "Çizğiçlar",                    "Rulers" },
  { "act.grid",        "Tör çiziqlar",                 "Gridlines" },
  { "act.status",      "Holat paneli",                 "Status bar" },
  { "act.thumb",       "Kiçik oyna",                   "Thumbnail" },
  { "act.fullscreen",  "Töliq ekran",                  "Full screen" },

  /* rasm */
  { "act.crop",        "Qirqiş",                       "Crop" },
  { "act.resize",      "Ölçam va qiyşaytiriş…",        "Resize and skew…" },
  { "act.rotright",    "Öngga 90° aylantiriş",         "Rotate right 90°" },
  { "act.rotleft",     "Çapga 90° aylantiriş",         "Rotate left 90°" },
  { "act.rot180",      "180° aylantiriş",              "Rotate 180°" },
  { "act.fliph",       "Gorizontal aks ettiriş",       "Flip horizontal" },
  { "act.flipv",       "Vertikal aks ettiriş",         "Flip vertical" },
  { "act.invert",      "Ranglarni teskari qiliş",      "Invert colors" },
  { "act.clear",       "Rasmni tozalaş",               "Clear image" },
  { "act.rotate",      "Aylantiriş",                   "Rotate" },

  /* sozlamalar */
  { "set.theme",       "Mavzu",                        "Theme" },
  { "set.system",      "Tizimga mos",                  "Follow system" },
  { "set.light",       "Yoruğ",                        "Light" },
  { "set.dark",        "Qoronği",                      "Dark" },
  { "set.lang",        "Til",                          "Language" },
  { "set.font",        "Interfeys şrifti…",            "Interface font…" },
  { "set.reset",       "Asl holiga qaytariş",          "Reset to default" },
  { "set.fonttitle",   "Interfeys şrifti",             "Interface font" },
  { "set.size",        "Ölçami:",                      "Size:" },
  { "set.family",      "Şrift:",                       "Font:" },
  { "set.preview",     "Namuna: Rasm çiziş — Öçirğiç, Şakllar", "Preview: The quick brown fox" },
  { "set.restart",     "Til dastur qayta oçilganda almaşadi. Hozir qayta oçilsinmi?",
                       "The language will change after restart. Restart now?" },
  { "set.later",       "Keyinroq",                     "Later" },
  { "set.restartnow",  "Qayta oçiş",                   "Restart" },

  /* yordam */
  { "help.keys",       "Qisqa tugmalar",               "Keyboard shortcuts" },
  { "help.about",      "Dastur haqida",                "About" },
  { "help.comment",    "Linux uçun yengil rasm çiziş dasturi",
                       "A lightweight drawing program for Linux" },
  { "help.keystext",
    "Ctrl+N — yangi rasm\nCtrl+O — oçiş\nCtrl+S — saqlaş\nCtrl+Shift+S — boşqa nom bilan saqlaş\n"
    "Ctrl+P — çop etiş\nCtrl+E — rasm xususiyatlari\nCtrl+Q — çiqiş\n\n"
    "Ctrl+Z — orqaga\nCtrl+Y yoki Ctrl+Shift+Z — oldinga\nCtrl+X / Ctrl+C / Ctrl+V — kesiş / nusxa / qöyiş\n"
    "Ctrl+A — hammasini tanlaş\nDelete — tanlanganni öçiriş\nCtrl+Shift+X — qirqiş\nCtrl+W — ölçam va qiyşaytiriş\n"
    "Ctrl+Shift+I — ranglarni teskari qiliş\n\n"
    "Ctrl+= / Ctrl+- — kattalaştiriş / kiçraytiriş\nCtrl + siçqonça ğildiragi — kattalaştiriş\n"
    "Ctrl+0 — oynaga moslaş\nCtrl+1 — asl ölçam\nCtrl+R — çizğiçlar\nCtrl+G — tör çiziqlar\nF11 — töliq ekran\n\n"
    "Enter yoki Esc — şakl/tanlovni joylaştiriş\nStrelkalar — tanlanganni 1 px siljitiş (Shift bilan 10 px)\n"
    "Shift — teng tomonli şakl, 15° qadam bilan buriş",
    "Ctrl+N — new image\nCtrl+O — open\nCtrl+S — save\nCtrl+Shift+S — save as\n"
    "Ctrl+P — print\nCtrl+E — image properties\nCtrl+Q — exit\n\n"
    "Ctrl+Z — undo\nCtrl+Y or Ctrl+Shift+Z — redo\nCtrl+X / Ctrl+C / Ctrl+V — cut / copy / paste\n"
    "Ctrl+A — select all\nDelete — delete selection\nCtrl+Shift+X — crop\nCtrl+W — resize and skew\n"
    "Ctrl+Shift+I — invert colors\n\n"
    "Ctrl+= / Ctrl+- — zoom in / zoom out\nCtrl + mouse wheel — zoom\n"
    "Ctrl+0 — fit to window\nCtrl+1 — actual size\nCtrl+R — rulers\nCtrl+G — gridlines\nF11 — full screen\n\n"
    "Enter or Esc — place shape/selection\nArrow keys — move selection by 1 px (10 px with Shift)\n"
    "Shift — keep proportions, rotate in 15° steps" },

  /* lenta guruhlari */
  { "grp.history",     "Tarix",                        "History" },
  { "grp.select",      "Tanlaş",                       "Selection" },
  { "grp.image",       "Rasm",                         "Image" },
  { "grp.tools",       "Asboblar",                     "Tools" },
  { "grp.brushes",     "Möyqalamlar",                  "Brushes" },
  { "grp.shapes",      "Şakllar",                      "Shapes" },
  { "grp.size",        "Qalinlik",                     "Size" },
  { "grp.colors",      "Ranglar",                      "Colors" },

  /* asboblar */
  { "tool.select",     "Törtburçak tanlaş",            "Rectangular selection" },
  { "tool.freeselect", "Erkin tanlaş",                 "Free-form selection" },
  { "tool.pencil",     "Qalam",                        "Pencil" },
  { "tool.fill",       "Böyaş",                        "Fill with color" },
  { "tool.text",       "Matn",                         "Text" },
  { "tool.eraser",     "Öçirğiç",                      "Eraser" },
  { "tool.picker",     "Rang oliş",                    "Color picker" },
  { "tool.magnifier",  "Lupa (çap tugma — yaqinlaştiriş, öng tugma — uzoqlaştiriş)",
                       "Magnifier (left click — zoom in, right click — zoom out)" },
  { "tool.brushmenu",  "Möyqalam turini tanlaş",       "Choose brush" },
  { "tool.selmenu",    "Tanlaş imkoniyatlari",         "Selection options" },

  /* mo'yqalamlar */
  { "brush.0",         "Möyqalam",                     "Brush" },
  { "brush.1",         "Kalligrafiya möyqalami",       "Calligraphy brush" },
  { "brush.2",         "Siyohli pero",                 "Fountain pen" },
  { "brush.3",         "Purkağiç",                     "Airbrush" },
  { "brush.4",         "Moy böyoq möyqalami",          "Oil brush" },
  { "brush.5",         "Rangli qalam",                 "Crayon" },
  { "brush.6",         "Marker",                       "Marker" },
  { "brush.7",         "Tabiiy qalam",                 "Natural pencil" },
  { "brush.8",         "Akvarel möyqalami",            "Watercolor brush" },

  /* shakllar */
  { "shape.0",  "Çiziq (Shift — 45° qadam)",               "Line (Shift — 45° steps)" },
  { "shape.1",  "Egri çiziq: çiziq çizing, keyin yaşil nuqtalarni sudrab egang",
                "Curve: draw a line, then drag the green points to bend it" },
  { "shape.2",  "Oval (Shift — doira)",                    "Oval (Shift — circle)" },
  { "shape.3",  "Törtburçak (Shift — kvadrat)",            "Rectangle (Shift — square)" },
  { "shape.4",  "Yumaloq törtburçak",                      "Rounded rectangle" },
  { "shape.5",  "Köpburçak: nuqtalarni bosing, tugatiş uçun ikki marta bosing yoki Enter",
                "Polygon: click points, double-click or press Enter to finish" },
  { "shape.6",  "Uçburçak",                                "Triangle" },
  { "shape.7",  "Töğri burçakli uçburçak",                 "Right triangle" },
  { "shape.8",  "Romb",                                    "Diamond" },
  { "shape.9",  "Beşburçak",                               "Pentagon" },
  { "shape.10", "Oltiburçak",                              "Hexagon" },
  { "shape.11", "Öngga strelka",                           "Right arrow" },
  { "shape.12", "Çapga strelka",                           "Left arrow" },
  { "shape.13", "Yuqoriga strelka",                        "Up arrow" },
  { "shape.14", "Pastga strelka",                          "Down arrow" },
  { "shape.15", "4 uçli yulduz",                           "Four-point star" },
  { "shape.16", "5 uçli yulduz",                           "Five-point star" },
  { "shape.17", "6 uçli yulduz",                           "Six-point star" },
  { "shape.18", "Yumaloq izoh",                            "Rounded rectangular callout" },
  { "shape.19", "Oval izoh",                               "Oval callout" },
  { "shape.20", "Bulut izoh",                              "Cloud callout" },
  { "shape.21", "Yurak",                                   "Heart" },
  { "shape.22", "Çaqmoq",                                  "Lightning" },

  /* chegara / ichi */
  { "ui.outline",      "Çegara:",                      "Outline:" },
  { "ui.fill",         "Içi:",                         "Fill:" },
  { "ui.none",         "Yöq",                          "None" },
  { "ui.solid1",       "1-rang",                       "Color 1" },
  { "ui.solid2",       "2-rang",                       "Color 2" },
  { "ui.color1",       "1-rang: çap tugma bilan çiziladi. Bosib, istalgan rangni tanlang.",
                       "Color 1: used with the left mouse button. Click to choose any color." },
  { "ui.color2",       "2-rang (fon rangi): öng tugma, şakl içi, öçirğiç va tanlov siljiganda böşagan joy uçun. Bosib, istalgan rangni tanlang.",
                       "Color 2 (background): right mouse button, shape fill, eraser and the area left by a moved selection. Click to choose any color." },
  { "ui.palette",      "Çap tugma — 1-rang, öng tugma — 2-rang",
                       "Left click — color 1, right click — color 2" },
  { "ui.editcolors",   "Ranglarni tahrirlaş",          "Edit colors" },
  { "ui.untitled",     "Nomsiz",                       "Untitled" },
  { "ui.ok",           "OK",                           "OK" },
  { "ui.cancel",       "Bekor qiliş",                  "Cancel" },
  { "ui.close",        "Yopiş",                        "Close" },
  { "ui.apply",        "Qöllaş",                       "Apply" },
  { "ui.images",       "Rasmlar",                      "Images" },
  { "ui.allfiles",     "Barça fayllar",                "All files" },

  /* saqlash so'rovi */
  { "save.ask",        "Özgarişlar saqlansinmi?",      "Save changes?" },
  { "save.lost",       "Saqlamasangiz, oxirgi özgarişlar yöqoladi.",
                       "If you don't save, your changes will be lost." },
  { "save.dont",       "Saqlamaslik",                  "Don't save" },
  { "err.open",        "Rasmni oçib bölmadi",          "Could not open the image" },
  { "err.save",        "Saqlab bölmadi",               "Could not save" },
  { "err.print",       "Çop etib bölmadi",             "Could not print" },
  { "err.wallpaper",   "Bu iş stolida fonni avtomatik özgartirib bölmadi. Rasm saqlandi:",
                       "Could not change the background on this desktop automatically. The image was saved to:" },
  { "ok.wallpaper",    "Iş stoli foni özgartirildi.",  "Desktop background changed." },
  { "err.noimage",     "Xotirada rasm yöq.",           "There is no image on the clipboard." },

  /* holat paneli */
  { "st.size",         "%d × %d px",                   "%d × %d px" },
  { "st.sel",          "Tanlov: %d × %d px",           "Selection: %d × %d px" },

  /* matn oynasi */
  { "text.new",        "Matn yoziş",                   "Add text" },
  { "text.edit",       "Matnni tahrirlaş",             "Edit text" },
  { "text.prompt",     "Matnni yozing:",               "Type your text:" },
  { "text.font",       "Şrift va ölçam:",              "Font and size:" },
  { "text.bg",         "Orqa fon (2-rang bilan)",      "Background (color 2)" },
  { "text.hint",       "Enter — tayyor,  Shift+Enter — yangi qator",
                       "Enter — done,  Shift+Enter — new line" },
  { "text.add",        "Qöşiş",                        "Add" },

  /* o'lcham oynasi */
  { "rs.title",        "Ölçam va qiyşaytiriş",         "Resize and skew" },
  { "rs.resize",       "Ölçamni özgartiriş",           "Resize" },
  { "rs.percent",      "Foizda",                       "Percentage" },
  { "rs.pixels",       "Pikselda",                     "Pixels" },
  { "rs.horiz",        "Gorizontal:",                  "Horizontal:" },
  { "rs.vert",         "Vertikal:",                    "Vertical:" },
  { "rs.keep",         "Nisbatni saqlaş",              "Maintain aspect ratio" },
  { "rs.skew",         "Qiyşaytiriş (daraja)",         "Skew (degrees)" },

  /* xususiyatlar */
  { "pr.title",        "Rasm xususiyatlari",           "Image properties" },
  { "pr.file",         "Fayl nomi:",                   "File name:" },
  { "pr.date",         "Saqlangan sana:",              "Last saved:" },
  { "pr.disk",         "Diskdagi hajmi:",              "Size on disk:" },
  { "pr.notsaved",     "Saqlanmagan",                  "Not saved" },
  { "pr.width",        "Eni (px):",                    "Width (px):" },
  { "pr.height",       "Böyi (px):",                   "Height (px):" },
  { "pr.colors",       "Ranglar:",                     "Colors:" },
  { "pr.color",        "Rangli",                       "Color" },
  { "pr.bw",           "Oq-qora",                      "Black and white" },
  { "pr.canvas",       "Kanvas ölçami",                "Canvas size" },
};

static GHashTable *str_table;

static const char *T(const char *k) {
  if (!str_table) {
    str_table = g_hash_table_new(g_str_hash, g_str_equal);
    for (gsize i = 0; i < G_N_ELEMENTS(STRS); i++)
      g_hash_table_insert(str_table, (gpointer)STRS[i].k, (gpointer)&STRS[i]);
  }
  const Str *s = g_hash_table_lookup(str_table, k);
  if (!s) {
    if (g_getenv("UZ_PAINT_DEBUG")) g_printerr("Tarjima yo'q: %s\n", k);
    return k;
  }
  return S.lang == 1 ? s->en : s->uz;
}

/* "brush.3" kabi kalitlar uchun */
static const char *Tn(const char *prefix, int n) {
  char key[32];
  g_snprintf(key, sizeof key, "%s.%d", prefix, n);
  return T(key);
}
