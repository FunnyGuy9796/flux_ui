#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libinput.h>
#include <fcntl.h>
#include <libudev.h>

#ifdef __linux__
#include <linux/input-event-codes.h>
#else
#define BTN_LEFT    0x110
#define BTN_RIGHT   0x111
#define BTN_MIDDLE  0x112
#define BTN_SIDE    0x113
#define BTN_EXTRA   0x114
#endif

typedef struct {
    struct libinput *li;
    struct udev *udev;
    int input_fd;

    double mouse_x, mouse_y;
    uint32_t mouse_button;

    uint32_t modifiers;
} InputState;

int input_init();
void input_process_event();
void input_cleanup();
int input_get_fd();
void input_get_mouse_pos(int *x, int *y);
uint32_t input_get_mouse_button();

#endif