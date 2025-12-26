#ifndef FLUX_API_H
#define FLUX_API_H

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define SOCKET_PATH "/tmp/flux_comp.sock"

typedef struct Window window_t;

typedef struct {
    int id;
    char request[256];
    window_t *window;
} WindowRequest;

unsigned long flux_create_window();
int flux_show_window(unsigned long win_id);
int flux_hide_window(unsigned long win_id);
int flux_render_window(unsigned long win_id);
int flux_destory_window(unsigned long win_id);

int flux_add_widget(unsigned long win_id, const char *widget_id);
int flux_set_widget_geometry(unsigned long win_id, const char *widget_id, float x, float y, float w, float h, int radius, int border_width);
int flux_set_widget_color(unsigned long win_id, const char *widget_id, const char color[32]);
int flux_set_widget_text(unsigned long win_id, const char *widget_id, const char *text);
int flux_set_widget_image(unsigned long win_id, const char *widget_id, const char *filename);
int flux_set_widget_font(unsigned long win_id, const char *widget_id, const char *filename);

#endif