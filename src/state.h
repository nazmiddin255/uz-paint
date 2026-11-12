/*
 * Uz Paint — Linux uchun yengil rasm chizish dasturi
 * Copyright (c) 2026 Uz Paint mualliflari
 * MIT litsenziyasi asosida tarqatiladi (LICENSE fayliga qarang).
 *
 * Umumiy turlar va dastur holati.
 */
#ifndef UZ_PAINT_STATE_H
#define UZ_PAINT_STATE_H

#include <gtk/gtk.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>

/* Eski Linux tizimlarida ham ishlashi uchun (glibc 2.34+) o'zimizning hypot */
static inline double uzp_hypot(double a, double b) { return sqrt(a * a + b * b); }
#define hypot(a, b) uzp_hypot((a), (b))

#define APP_ID      "uz-paint"
#define APP_NAME    "Uz Paint"
#define APP_VERSION "2.0.0"

#define PAD       24.0     /* kanvas atrofidagi bo'sh joy (ekran px) */
#define ROT_OFF   28.0     /* burish tutqichi masofasi (ekran px) */
#define HANDLE_R  8.0      /* tutqich sezgirligi (ekran px) */
#define MAX_UNDO  60
#define UNDO_BYTES ((gsize)600 * 1024 * 1024)

typedef enum {
  T_SELECT, T_FREESELECT, T_PENCIL, T_FILL, T_TEXT, T_ERASER,
  T_PICKER, T_MAGNIFIER, T_BRUSH, T_SHAPE, T_COUNT
} Tool;

typedef enum {
  B_NORMAL, B_CALLIGRAPHY, B_PEN, B_AIRBRUSH, B_OIL, B_CRAYON,
  B_MARKER, B_NATURAL, B_WATERCOLOR, B_COUNT
} BrushKind;

typedef enum {
  SH_LINE, SH_CURVE, SH_OVAL, SH_RECT, SH_RRECT, SH_POLYGON,
  SH_TRIANGLE, SH_RTRIANGLE, SH_DIAMOND, SH_PENTAGON, SH_HEXAGON,
  SH_ARROW_R, SH_ARROW_L, SH_ARROW_U, SH_ARROW_D,
  SH_STAR4, SH_STAR5, SH_STAR6,
  SH_CALLOUT_RRECT, SH_CALLOUT_OVAL, SH_CALLOUT_CLOUD,
  SH_HEART, SH_LIGHTNING, SH_COUNT
} ShapeKind;

/* belgilar (ikonlar) raqamlari */
enum {
  IC_SELECT = 0, IC_FREESELECT, IC_PENCIL, IC_FILL, IC_TEXT, IC_ERASER,
  IC_PICKER, IC_MAGNIFIER, IC_CROP, IC_RESIZE, IC_ROTATE, IC_SELMENU, IC_UNDO, IC_REDO,
  IC_BRUSH0 = 100,          /* + BrushKind */
  IC_SHAPE0 = 200           /* + ShapeKind */
};

typedef enum { O_SHAPE, O_POINTS, O_TEXT, O_IMAGE } ObjKind;

typedef enum {
  D_NONE, D_DRAW, D_CREATE, D_MOVE, D_RESIZE, D_ROTATE, D_POINT,
  D_SELRECT, D_LASSO, D_CANVAS, D_THUMB
} DragMode;

/* Hali rasmga "yopishtirilmagan", tahrirlanadigan obyekt:
 * shakl, chiziq/egri chiziq, matn yoki tanlangan rasm bo'lagi. */
typedef struct {
  ObjKind kind;
  int shape;
  double x, y, w, h, angle;      /* O_SHAPE / O_TEXT / O_IMAGE */
  double pts[8];                 /* O_POINTS: chiziq (2) yoki egri chiziq (4 nuqta) */
  int npts;
  double *poly;                  /* ko'pburchak: 0..1 oralig'idagi nuqtalar */
  int npoly;
  GdkRGBA stroke, fill;
  double width;
  gboolean outline, filled;
  char *text, *font;
  gboolean text_bg;
  cairo_surface_t *img;          /* tanlangan bo'lak */
  cairo_surface_t *img_key;      /* shaffof tanlash uchun tayyorlangan nusxa */
  GdkRGBA key_color;
  gboolean lifted;               /* rasmdan kesib olingan */
} Obj;

typedef struct {
  /* oyna va vidjetlar */
  GtkWidget *win, *vbox, *menubar, *ribbon_scroll, *ribbon, *grid, *overlay, *scroll, *area;
  GtkWidget *hruler, *vruler, *corner, *thumb_frame, *thumb, *statusbar;
  GtkWidget *st_pos, *st_sel, *st_size, *st_file, *zoom_scale, *zoom_label;
  GtkWidget *size_spin, *size_box, *outline_combo, *fill_combo, *cbtn1, *cbtn2, *palette;
  GtkWidget *brush_btn, *brush_icon;
  GtkWidget *btn_tool[T_COUNT], *btn_shape[SH_COUNT];
  GtkWidget *mi_rulers, *mi_grid, *mi_status, *mi_thumb, *mi_transparent, *recent_menu;
  GtkWidget *mi_theme[3], *mi_lang[2];
  GPtrArray *icons;
  GtkAccelGroup *accel;
  GtkCssProvider *css;

  /* hujjat */
  cairo_surface_t *surf;
  int W, H;
  cairo_surface_t *layer;        /* marker / akvarel uchun vaqtinchalik qatlam */
  double layer_alpha;
  double zoom;
  Tool tool, prev_tool;
  int prev_sub;
  int brush, shape;
  GdkRGBA c1, c2;
  Obj *fl;

  /* sichqoncha holati */
  DragMode drag;
  int handle, btn;
  double sx, sy, lx, ly, ax, ay;
  double mouse_x, mouse_y;
  gboolean mouse_in;
  Obj orig;
  GArray *path;                  /* lasso / ko'pburchak nuqtalari */
  gboolean poly_active;
  int canvas_handle;
  double new_w, new_h;

  /* bekor qilish */
  GList *undo, *redo;
  gsize undo_bytes;

  char *filename;
  gboolean dirty;

  /* mo'yqalam holati */
  double pen_w;
  double bristle[16][4];
  int nbristle;
  guint air_timer;
  double sizes[4];

  /* ko'rinish */
  gboolean rulers, grid_on, status_on, thumb_on, transparent_sel, fullscreen;
  gboolean pending_anchor;
  double anchor_ix, anchor_iy, anchor_vx, anchor_vy;

  /* sozlamalar */
  char *ui_family;
  int ui_size;
  int theme;                     /* 0 tizimga mos, 1 yorug', 2 qorong'i */
  int lang;                      /* 0 o'zbekcha, 1 inglizcha */
  char *text_font;
  char *orig_theme;
  gboolean sys_dark;
  GdkRGBA custom[10];
  int ncustom;
  char *recent[8];
  int nrecent;
  int win_w, win_h;
  gboolean updating;
} State;

static State S;

#endif
