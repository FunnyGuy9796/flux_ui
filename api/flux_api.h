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

#endif