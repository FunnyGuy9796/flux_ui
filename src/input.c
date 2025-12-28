#include "input.h"
#include "compositor.h"

static InputState input_state = {0};

static int open_restricted(const char *path, int flags, void *user_data) {
    int fd = open(path, flags);

    if (fd < 0)
        printf("  WW: (input.c) open_restricted() -> failed to open %s\n", path);

    return fd < 0 ? -1 : fd;
}

static void close_restricted(int fd, void *user_data) {
    close(fd);
}

static const struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted
};

int input_init() {
    input_state.udev = udev_new();

    if (!input_state.udev) {
        printf("  EE: (input.c) input_init() -> failed to create udev context\n");

        return 1;
    }

    input_state.li = libinput_udev_create_context(&interface, NULL, input_state.udev);

    if (!input_state.li) {
        printf("  EE: (input.c) input_init() -> failed to create libinput context\n");
        udev_unref(input_state.udev);

        return 1;
    }

    if (libinput_udev_assign_seat(input_state.li, "seat0") != 0) {
        printf("  EE: (input.c) input_init() -> failed to assign seat\n");
        libinput_unref(input_state.li);
        udev_unref(input_state.udev);

        return 1;
    }

    input_state.input_fd = libinput_get_fd(input_state.li);

    if (input_state.input_fd < 0) {
        printf("  EE: (input.c) input_init() -> failed to get libinput fd\n");
        libinput_unref(input_state.li);
        udev_unref(input_state.udev);

        return 1;
    }

    int flags = fcntl(input_state.input_fd, F_GETFL, 0);

    fcntl(input_state.input_fd, F_SETFL, flags | O_NONBLOCK);

    input_state.mouse_x = mode->hdisplay / 2.0;
    input_state.mouse_y = mode->vdisplay / 2.0;

    printf("  II: (input.c) input_init() -> input system... [OK]\n");

    return 0;
}

static void handle_pointer_motion(struct libinput_event_pointer *pointer_event) {
    double dx = libinput_event_pointer_get_dx(pointer_event);
    double dy = libinput_event_pointer_get_dy(pointer_event);

    input_state.mouse_x += dx;
    input_state.mouse_y += dy;

    if (input_state.mouse_x < 0)
        input_state.mouse_x = 0;

    if (input_state.mouse_y < 0)
        input_state.mouse_y = 0;

    if (input_state.mouse_x >= mode->hdisplay)
        input_state.mouse_x = mode->hdisplay - 1;

    if (input_state.mouse_y >= mode->vdisplay)
        input_state.mouse_y = mode->vdisplay - 1;

    comp_on_mouse_move((int)input_state.mouse_x, (int)input_state.mouse_y);
}

static void handle_pointer_button(struct libinput_event_pointer *pointer_event) {
    uint32_t button = libinput_event_pointer_get_button(pointer_event);
    enum libinput_button_state state = libinput_event_pointer_get_button_state(pointer_event);
    uint32_t button_bit = 0;

    switch (button) {
        case BTN_LEFT: {
            button_bit = 1 << 0;

            break;
        }

        case BTN_RIGHT: {
            button_bit = 1 << 1;

            break;
        }

        case BTN_MIDDLE: {
            button_bit = 1 << 2;

            break;
        }
    }

    if (state == LIBINPUT_BUTTON_STATE_PRESSED) {
        input_state.mouse_button |= button_bit;

        comp_on_mouse_down((int)input_state.mouse_x, (int)input_state.mouse_y, button);
    } else {
        input_state.mouse_button &= ~button_bit;

        comp_on_mouse_up((int)input_state.mouse_x, (int)input_state.mouse_y, button);
    }
}

static void handle_pointer_axis(struct libinput_event_pointer *pointer_event) {
    if (libinput_event_pointer_has_axis(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
        double value = libinput_event_pointer_get_axis_value(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);

        comp_on_scroll(0, (int)(value * 10));
    }

    if (libinput_event_pointer_has_axis(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
        double value = libinput_event_pointer_get_axis_value(pointer_event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);

        comp_on_scroll((int)(value * 10), 0);
    }
}

static void handle_keyboard_key(struct libinput_event_keyboard *keyboard_event) {
    uint32_t key = libinput_event_keyboard_get_key(keyboard_event);
    enum libinput_key_state state = libinput_event_keyboard_get_key_state(keyboard_event);

    if (state == LIBINPUT_KEY_STATE_PRESSED)
        comp_on_key_down(key, input_state.modifiers);
    else if (state == LIBINPUT_KEY_STATE_RELEASED)
        comp_on_key_up(key, input_state.modifiers);
}

void input_process_event() {
    libinput_dispatch(input_state.li);

    struct libinput_event *event;

    while ((event = libinput_get_event(input_state.li)) != NULL) {
        enum libinput_event_type type = libinput_event_get_type(event);

        switch (type) {
            case LIBINPUT_EVENT_POINTER_MOTION: {
                handle_pointer_motion(libinput_event_get_pointer_event(event));

                break;
            }

            case LIBINPUT_EVENT_POINTER_BUTTON: {
                handle_pointer_button(libinput_event_get_pointer_event(event));

                break;
            }

            case LIBINPUT_EVENT_POINTER_AXIS: {
                handle_pointer_axis(libinput_event_get_pointer_event(event));

                break;
            }

            case LIBINPUT_EVENT_KEYBOARD_KEY: {
                handle_keyboard_key(libinput_event_get_keyboard_event(event));

                break;
            }

            case LIBINPUT_EVENT_DEVICE_ADDED: {
                printf("  II: (input.c) input_process_event() -> input device added\n");

                break;
            }

            case LIBINPUT_EVENT_DEVICE_REMOVED: {
                printf("  II: (input.c) input_process_event() -> input device removed\n");

                break;
            }

            default:
                break;
        }

        libinput_event_destroy(event);
    }
}

void input_cleanup() {
    if (input_state.li) {
        libinput_unref(input_state.li);

        input_state.li = NULL;
    }

    if (input_state.udev) {
        udev_unref(input_state.udev);

        input_state.udev = NULL;
    }
}

int input_get_fd() {
    return input_state.input_fd;
}

void input_get_mouse_pos(int *x, int *y) {
    if (x)
        *x = (int)input_state.mouse_x;

    if (y)
        *y = (int)input_state.mouse_y;
}

uint32_t input_get_mouse_button() {
    return input_state.mouse_button;
}