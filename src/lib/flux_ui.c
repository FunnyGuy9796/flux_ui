#include "flux_ui.h"
#include "GLES2/gl2.h"
#include "stb_image.h"
#include "stb_truetype.h"
#include "../compositor.h"

static unsigned int counter = 0;

GLuint ui_load_texture(const char *filename) {
    int width, height, channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 4);

    if (!data) {
        printf("  EE: load_texture() -> failed to load image: %s\n", filename);

        return 0;
    }

    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);

    return tex;
}

font_t *ui_load_font(const char *ttf_path, float pixel_height) {
    font_t *font = malloc(sizeof(font_t));
    unsigned char *ttf_buffer;
    size_t size;

    FILE *f = fopen(ttf_path, "rb");

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    rewind(f);

    ttf_buffer = malloc(size);
    fread(ttf_buffer, 1, size, f);
    fclose(f);

    const int ATLAS_W = 512;
    const int ATLAS_H = 512;
    unsigned char *bitmap = calloc(ATLAS_W * ATLAS_H, 1);

    stbtt_fontinfo info;

    stbtt_InitFont(&info, ttf_buffer, 0);

    stbtt_bakedchar baked[128];

    stbtt_BakeFontBitmap(ttf_buffer, 0, pixel_height, bitmap, ATLAS_W, ATLAS_H, 32, 96, baked);

    glGenTextures(1, &font->texture);
    glBindTexture(GL_TEXTURE_2D, font->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_W, ATLAS_H, 0, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    for (int i = 32; i < 128; i++) {
        stbtt_bakedchar *b = &baked[i - 32];
        glyph_t *g = &font->glyphs[i];

        g->u0 = b->x0 / (float)ATLAS_W;
        g->v0 = b->y0 / (float)ATLAS_H;
        g->u1 = b->x1 / (float)ATLAS_W;
        g->v1 = b->y1 / (float)ATLAS_H;

        g->w = b->x1 - b->x0;
        g->h = b->y1 - b->y0;
        g->xoff = b->xoff;
        g->yoff = b->yoff;
        g->xadvance = b->xadvance;
    }

    free(bitmap);
    free(ttf_buffer);

    font->size = pixel_height;

    return font;
}

void ui_draw_rect(float x, float y, float w, float h, float r, float red, float green, float blue, float alpha) {
    float verts[] = {
        0, 0,
        1, 0,
        0, 1,
        1, 1,
    };

    glUseProgram(program);
    
    glUniform1i(uni_use_texture, 0);
    glUniform2f(uni_rect_pos, x, y);
    glUniform2f(uni_rect_size, w, h);
    glUniform2f(uni_screen_size, (float)mode->hdisplay, (float)mode->vdisplay);
    glUniform4f(uni_color, red, green, blue, alpha);
    glUniform1f(uni_radius, r);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(attr_pos);
    glVertexAttribPointer(attr_pos, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void ui_draw_rect_texture(float x, float y, float w, float h, float r, float red, float green, float blue, float alpha, GLuint texture) {
    float verts[] = {
        0, 0,
        1, 0,
        0, 1,
        1, 1,
    };

    glUseProgram(program);
    
    glUniform1i(uni_use_texture, 1);
    glUniform2f(uni_rect_pos, x, y);
    glUniform2f(uni_rect_size, w, h);
    glUniform2f(uni_screen_size, (float)mode->hdisplay, (float)mode->vdisplay);
    glUniform4f(uni_color, red, green, blue, alpha);
    glUniform1f(uni_radius, r);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    glUniform1i(uni_tex, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(attr_pos);
    glVertexAttribPointer(attr_pos, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void draw_glyph(float x, float y, float w, float h, float u0, float v0, float u1, float v1, float red, float green, float blue, float alpha, GLuint font_texture){
    float verts[] = {
        0, 0, u0, v0,
        1, 0, u1, v0,
        0, 1, u0, v1,
        1, 1, u1, v1
    };

    glUseProgram(text_program);

    glUniform2f(text_uni_glyph_pos, x, y);
    glUniform2f(text_uni_glyph_size, w, h);
    glUniform2f(text_uni_screen_size, mode->hdisplay, mode->vdisplay);
    glUniform4f(text_uni_color, red, green, blue, alpha);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glUniform1i(text_uni_tex, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(text_attr_pos);
    glEnableVertexAttribArray(text_attr_uv);

    glVertexAttribPointer(text_attr_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glVertexAttribPointer(text_attr_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void ui_draw_text(float x, float y, font_t *font, const char *text, float r, float g, float b, float a) {
    float pen_x = x;
    float pen_y = y;

    while (*text) {
        char c = *text++;

        if (c == '\n') {
            pen_x = x;
            pen_y += font->size;

            continue;
        }

        if (c < 32 || c >= 128)
            continue;
        
        glyph_t *glyph = &font->glyphs[(int)c];

        float gx = pen_x + glyph->xoff;
        float gy = pen_y + glyph->yoff;

        draw_glyph(gx, gy, glyph->w, glyph->h, glyph->u0, glyph->v0, glyph->u1, glyph->v1, r, g, b, a, font->texture);

        pen_x += glyph->xadvance;
    }
}

void ui_measure_text(const char *text, font_t *font, float *out_width, float *out_height, float *out_visual_min_y) {
    float pen_x = 0.0f;
    float pen_y = 0.0f;
    float max_width = 0.0f;
    float visual_min_y = 1e6f;
    float visual_max_y = -1e6f;
    float line_height = font->size;

    while (*text) {
        char c = *text++;

        if (c == '\n') {
            if (pen_x > max_width)
                max_width = pen_x;

            pen_x = 0.0f;
            pen_y += line_height;

            continue;
        }

        if (c < 32 || c >= 128)
            continue;

        glyph_t *g = &font->glyphs[(int)c];

        pen_x += g->xadvance;

        float glyph_top = g->yoff;
        float glyph_bottom = g->yoff + g->h;

        if (glyph_top < visual_min_y)
            visual_min_y = glyph_top;

        if (glyph_bottom > visual_max_y)
            visual_max_y = glyph_bottom;
    }

    if (pen_x > max_width)
        max_width = pen_x;

    float total_height = visual_max_y - visual_min_y;

    if (out_width)
        *out_width = max_width;

    if (out_height)
        *out_height = total_height;

    if (out_visual_min_y)
        *out_visual_min_y = visual_min_y;
}

window_t *ui_create_window() {
    int width = mode->hdisplay;
    int height = mode->vdisplay;

    window_t *window = calloc(1, sizeof(window_t));
    window->widget_count = 0;
    window->has_focus = false;
    window->rendered = false;
    window->width = width;
    window->height = height;
    window->id = counter++;

    glGenTextures(1, &window->color_tex);
    glBindTexture(GL_TEXTURE_2D, window->color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &window->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, window->fbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, window->color_tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        printf("  EE: ui_create_window() -> window framebuffer incomplete (0x%x)\n", status);

        exit(1);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return window;
}

static void widget_get_world_pos(widget_t *widget, float *x, float *y) {
    float pos_x = widget->x;
    float pos_y = widget->y;

    while (widget->parent.type == PARENT_WIDGET) {
        widget = widget->parent.widget;
        pos_x += widget->x;
        pos_y += widget->y;
    }

    *x = pos_x;
    *y = pos_y;
}

static void render_widget(widget_t *widget) {
    float r, g, b, a;
    float pos_x, pos_y;

    widget_get_world_pos(widget, &pos_x, &pos_y);

    if (strlen(widget->color) != 0)
        ui_hex_to_rgba(widget->color, &r, &g, &b, &a);

    if (widget->parent.type == PARENT_WIDGET) {
        int x, y, w, h;
        widget_t *parent = widget->parent.widget;

        x = (int)parent->x;
        y = (int)parent->y;
        w = (int)parent->w;
        h = (int)parent->h;

        y = mode->vdisplay - (y + h);

        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, w, h);
    }

    switch (widget->type) {
        case WIDGET_RECT: {
            ui_draw_rect(pos_x, pos_y, widget->w, widget->h, widget->radius, r, g, b, a);

            break;
        }

        case WIDGET_TEXT: {
            ui_draw_text(pos_x, pos_y, widget->font, widget->text, r, g, b, a);

            break;
        }

        case WIDGET_IMAGE: {
            ui_draw_rect_texture(pos_x, pos_y, widget->w, widget->h, widget->radius, r, g, b, a, widget->texture);

            break;
        }

        case WIDGET_NONE:
            break;
    }

    glDisable(GL_SCISSOR_TEST);

    for (int i = 0; i < widget->child_count; i++)
        render_widget(widget->children[i]);
}

void ui_render_window(window_t *window) {
    if (!window || window->widget_count <= 0 || !window->rendered)
        return;

    glBindFramebuffer(GL_FRAMEBUFFER, window->fbo);
    glViewport(0, 0, window->width, window->height);

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    for (int i = 0; i < window->widget_count; i++)
        render_widget(window->widgets[i]);
}

void ui_destroy_window(window_t *window) {
    for (int i = 0; i < window->widget_count; i++)
        ui_destroy_widget(window->widgets[i]);

    memset(window->widgets, 0, sizeof(window->widgets));

    window->widget_count = 0;
    window->has_focus = false;

    free(window);
}

bool ui_window_get_rendered(window_t *window) {
    return window->rendered;
}

unsigned long ui_window_get_id(window_t *window) {
    return window->id;
}

GLuint ui_window_get_texture(window_t *window) {
    return window->color_tex;
}

widget_t *ui_window_get_widget(window_t *window, const char *widget_id) {
    for (int i = 0; i < window->widget_count; i++) {
        widget_t *curr_widg = window->widgets[i];

        if (strcmp(curr_widg->id, widget_id) == 0)
            return curr_widg;
    }

    return NULL;
}

widget_t *ui_create_widget(const char id[64], widget_type_t type) {
    widget_t *widg = calloc(1, sizeof(widget_t));
    
    strcpy(widg->id, id);
    widg->type = type;
    widg->children = calloc(MAX_CHILDREN, sizeof(widget_t *));
    widg->child_count = 0;
    widg->parent.type = PARENT_NONE;
    widg->parent.widget = NULL;
    widg->parent.window = NULL;

    return widg;
}

void ui_destroy_widget(widget_t *widget) {
    if (!widget)
        return;

    for (int i = 0; i < widget->child_count; i++)
        ui_destroy_widget(widget->children[i]);

    free(widget->children);
    free(widget);
}

void ui_widget_set_geometry(widget_t *widg, float x, float y, float w, float h, float radius) {
    if (x > -1)
        widg->x = x;

    if (y > -1)
        widg->y = y;

    if (w > -1)
        widg->w = w;

    if (h > -1)
        widg->h = h;

    if (radius > -1)
        widg->radius = radius;
}

void ui_widget_set_color(widget_t *widg, const char *color) {
    strcpy(widg->color, color);
}

void ui_widget_set_text(widget_t *widg, const char *text) {
    if (widg->type != WIDGET_NONE && widg->type != WIDGET_TEXT) {
        printf("  WW: ui_widget_set_text() -> widget has a different content type, ignoring set_text\n    widget ID: %s\n", widg->id);

        return;
    }

    strcpy(widg->text, text);
}

void ui_widget_set_image(widget_t *widg, GLuint texture) {
    if (widg->type != WIDGET_NONE && widg->type != WIDGET_IMAGE) {
        printf("  WW: ui_widget_set_image() -> widget has a different content type, ignoring set_image\n    widget ID: %s\n", widg->id);

        return;
    }

    widg->texture = texture;
}

void ui_widget_set_font(widget_t *widg, font_t *font) {
    widg->font = font;
}

font_t *ui_widget_get_font(widget_t *widg) {
    return widg->font;
}

void ui_widget_append_child(widget_t *widg, widget_t *child) {
    if (!widg) {
        printf("  EE: ui_widget_append_child() -> attempted to append to an invalid widget\n");

        exit(1);
    }

    if (!child) {
        printf("  EE: ui_widget_append_child() -> attempted to append an invalid child\n");

        exit(1);
    }

    int count = widg->child_count;

    if (count >= MAX_CHILDREN) {
        printf("  EE: ui_widget_append_child() -> allowed number of children exceeded\n");

        exit(1);
    }

    for (int i = 0; i < count; i++) {
        widget_t *curr_widg = widg->children[i];

        if (strcmp(curr_widg->id, child->id) == 0) {
            widg->children[i] = child;

            return;
        }
    }

    widget_parent_t widg_parent;
    widg_parent.type = PARENT_WIDGET;
    widg_parent.widget = widg;

    widg->children[count] = child;
    widg->child_count++;
    child->parent = widg_parent;
}

void ui_append_widget(window_t *window, widget_t *widget) {
    if (!window) {
        printf("  EE: ui_append_widget() -> attempted to append to an invalid window\n");

        exit(1);
    }

    if (!widget) {
        printf("  EE: ui_append_widget() -> attempted to append an invalid widget\n");

        exit(1);
    }

    int count = window->widget_count;

    if (count >= MAX_WIDGETS) {
        printf("  EE: ui_append_widget() -> allowed number of widgets exceeded\n");

        exit(1);
    }

    if (widget->color[0] == '\0')
        ui_widget_set_color(widget, "#ffffffff");

    for (int i = 0; i < count; i++) {
        widget_t *curr_widg = window->widgets[i];

        if (strcmp(curr_widg->id, widget->id) == 0) {
            window->widgets[i] = widget;

            return;
        }
    }

    widget_parent_t widg_parent;
    widg_parent.type = PARENT_WINDOW;
    widg_parent.window = window;

    window->widgets[count] = widget;
    window->widget_count++;
    widget->parent = widg_parent;
}

void ui_remove_widget(window_t *window, widget_t *widget) {
    if (!window) {
        printf("  EE: ui_remove_widget() -> attempted to remove from an invalid window\n");

        exit(1);
    }

    if (!widget) {
        printf("  EE: ui_remove_widget() -> attempted to remove an invalid widget\n");

        exit(1);
    }

    int count = window->widget_count;

    for (int i = 0; i < count; i++) {
        widget_t *curr_widg = window->widgets[i];

        if (strcmp(curr_widg->id, widget->id) == 0) {
            window->widgets[i] = window->widgets[count - 1];
            window->widgets[count - 1] = NULL;
            window->widget_count--;
        }
    }
}

void ui_request_render(window_t *window) {
    if (!window)
        return;

    window->rendered = true;
}

void ui_request_hide(window_t *window) {
    if (!window)
        return;

    window->rendered = false;
}

void ui_set_render_loop(window_t *window, window_render_loop_fn func) {
    if (!window)
        return;

    window->render_loop = func;
}

void ui_call_render_loop(window_t *window, float dt) {
    if (!window || !window->render_loop || !window->rendered)
        return;

    window->render_loop(window, dt);
}