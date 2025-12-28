#ifndef FLUX_UI_H
#define FLUX_UI_H

#define STB_IMAGE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "flux_type.h"

#define MAX_WIDGETS 256
#define MAX_CHILDREN 32

typedef struct Glyph {
    float u0, v0;
    float u1, v1;
    float xoff, yoff;
    float xadvance;
    float w, h;
} glyph_t;

typedef struct Font {
    GLuint texture;
    glyph_t glyphs[128];
    float size;
} font_t;

typedef struct Window window_t;
typedef struct Widget widget_t;

typedef void (*widget_enter_fn)(widget_t *self);
typedef void (*widget_leave_fn)(widget_t *self);
typedef void (*widget_button_down_fn)(widget_t *self);
typedef void (*widget_button_up_fn)(widget_t *self);
typedef void (*widget_key_down_fn)(widget_t *self);
typedef void (*widget_key_up_fn)(widget_t *self);

typedef void (*window_render_loop_fn)(window_t *self, float dt);
typedef void (*window_exit_fn)(window_t *self);

typedef enum {
    PARENT_NONE,
    PARENT_WIDGET,
    PARENT_WINDOW
} parent_type_t;

typedef struct {
    parent_type_t type;

    union {
        struct Widget *widget;
        struct Window *window;
    };
} widget_parent_t;

typedef struct Widget {
    float x, y, w, h;
    int radius, border_width;
    char color[32];
    char text[256];
    font_t *font;
    GLuint texture;
    char id[64];
    widget_type_t type;
    struct Widget **children;
    int child_count;
    widget_parent_t parent;

    widget_enter_fn on_mouse_enter;
    widget_leave_fn on_mouse_leave;
    widget_button_down_fn on_mouse_btn_down;
    widget_button_up_fn on_mouse_btn_up;
} widget_t;

typedef struct Window {
    widget_t *widgets[MAX_WIDGETS];
    int widget_count;
    bool has_focus;
    bool rendered;
    unsigned long id;

    GLuint fbo;
    GLuint color_tex;
    GLuint depth_rbo;
    int width, height;

    window_render_loop_fn render_loop;
    window_exit_fn on_exit;
} window_t;

inline void ui_hex_to_rgba(const char *hex, float *r, float *g, float *b, float *a) {
    unsigned int alpha, red, green, blue;

    if (sscanf(hex, "#%2x%2x%2x%2x", &red, &green, &blue, &alpha) != 4) {
        printf("  WW: hex_to_rgba() -> failed to parse hex string: %s\n", hex);

        return;
    }
    
    *a = alpha / 255.0f;
    *r = red / 255.0f;
    *g = green / 255.0f;
    *b = blue / 255.0f;
}

GLuint ui_load_texture(const char *filename);
font_t *ui_load_font(const char *ttf_path, float pixel_height);

void ui_draw_rect(float x, float y, float w, float h, float r, float red, float green, float blue, float alpha);
void ui_draw_rect_texture(float x, float y, float w, float h, float r, float red, float green, float blue, float alpha, GLuint texture);
void ui_draw_text(float x, float y, font_t *font, const char *text, float r, float g, float b, float a);
void ui_measure_text(const char *text, font_t *font, float *out_width, float *out_height, float *out_visual_min_y);

window_t *ui_create_window();
void ui_render_window(window_t *window);
void ui_destroy_window(window_t *window);
bool ui_window_get_rendered(window_t *window);
unsigned long ui_window_get_id(window_t *window);
GLuint ui_window_get_texture(window_t *window);
widget_t *ui_window_get_widget(window_t *window, const char *widget_id);
void ui_call_render_loop(window_t *window, float dt);

widget_t *ui_create_widget(const char *id, widget_type_t type);
void ui_destroy_widget(widget_t *widget);
void ui_widget_set_geometry(widget_t *widg, float x, float y, float w, float h, float radius);
void ui_widget_set_color(widget_t *widg, const char *color);
void ui_widget_set_text(widget_t *widg, const char *text);
void ui_widget_set_image(widget_t *widg, GLuint texture);
void ui_widget_set_font(widget_t *widg, font_t *font);
font_t *ui_widget_get_font(widget_t *widg);
void ui_widget_append_child(widget_t *widg, widget_t *child);
void ui_append_widget(window_t *window, widget_t *widget);
void ui_remove_widget(window_t *window, widget_t *widget);

void ui_request_render(window_t *window);
void ui_request_hide(window_t *window);
void ui_set_render_loop(window_t *window, window_render_loop_fn func);

#endif