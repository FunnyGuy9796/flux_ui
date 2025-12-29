#include "flux_api.h"

static int flux_sock_fd = -1;
static struct sockaddr_un flux_addr;

static int send_request(WindowRequest *req, void *response, size_t response_size) {
    if (flux_sock_fd == -1) {
        if (flux_init() != 0)
            return 1;
    }

    ssize_t sent = send(flux_sock_fd, req, sizeof(*req), 0);

    if (sent != sizeof(*req)) {
        printf("  EE: (flux_api.c) send_request() -> failed to send request\n");

        return 1;
    }

    if (response && response_size > 0) {
        ssize_t received = recv(flux_sock_fd, response, response_size, 0);

        if (received < 0) {
            printf("  EE: (flux_api.c) send_request() -> failed to receive response\n");
            return 1;
        }

        if (received < response_size)
            ((char *)response)[received] = '\0';
    }

    return 0;
}

int flux_init() {
    if (flux_sock_fd != -1)
        return 0;

    flux_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (flux_sock_fd < 0) {
        printf("  EE: (flux_api.c) flux_init() -> failed to create the socket\n");

        return 1;
    }

    memset(&flux_addr, 0, sizeof(flux_addr));

    flux_addr.sun_family = AF_UNIX;

    strncpy(flux_addr.sun_path, SOCKET_PATH, sizeof(flux_addr.sun_path) - 1);

    if (connect(flux_sock_fd, (struct sockaddr *)&flux_addr, sizeof(flux_addr)) < 0) {
        printf("  EE: (flux_api.c) flux_init() -> failed to connect to the socket");

        close(flux_sock_fd);

        flux_sock_fd = -1;

        return 1;
    }

    return 0;
}

void flux_shutdown(unsigned long win_id) {
    WindowRequest req;
    
    memset(&req, 0, sizeof(req));

    req.id = win_id;

    strncpy(req.request, "SHUTDOWN", sizeof(req.request) - 1);

    if (send_request(&req, NULL, 0) < 0) {
        printf("  EE: (flux_api.c) flux_shutdown() -> failed to close connection to compositor\n");

        return;
    }

    close(flux_sock_fd);

    flux_sock_fd = -1;
}

unsigned long flux_create_window() {
    WindowRequest req;
    unsigned long win_id = 0;

    memset(&req, 0, sizeof(req));

    req.id = 0;

    strncpy(req.request, "CREATE_WINDOW", sizeof(req.request) - 1);

    if (send_request(&req, &win_id, sizeof(win_id)) < 0) {
        printf("  EE: (flux_api.c) flux_create_window() -> failed to create window\n");

        return 0;
    }

    return win_id;
}

int flux_show_window(unsigned long win_id) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    strncpy(req.request, "SHOW", sizeof(req.request) - 1);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("  EE: (flux_api.c) flux_show_window() -> failed to show window %lu\n", win_id);

        return 1;
    }

    return 0;
}

int flux_hide_window(unsigned long win_id) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    strncpy(req.request, "HIDE", sizeof(req.request) - 1);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("  EE: (flux_api.c) flux_hide_window() -> failed to hide window %lu\n", win_id);

        return 1;
    }

    return 0;
}

int flux_render_window(unsigned long win_id) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    strncpy(req.request, "RENDER", sizeof(req.request) - 1);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("  EE: (flux_api.c) flux_render_window() -> failed to render window %lu\n", win_id);

        return 1;
    }

    return 0;
}

int flux_add_widget(unsigned long win_id, const char *widget_id, widget_type_t type) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    snprintf(req.request, sizeof(req.request), "CREATE_WIDGET:%s:%u", widget_id, type);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("  EE: (flux_api.c) flux_add_widget() -> failed to add widget %s\n", widget_id);

        return 1;
    }

    return 0;
}

int flux_set_widget_geometry(unsigned long win_id, const char *widget_id, float x, float y, float w, float h, int radius, int border_width) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    snprintf(req.request, sizeof(req.request), "SET_WIDGET_GEOMETRY:%s:%f:%f:%f:%f:%d:%d", widget_id, x, y, w, h, radius, border_width);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("  EE: (flux_api.c) flux_set_widget_geometry() -> failed to set geometry for widget %s\n", widget_id);

        return 1;
    }

    return 0;
}

int flux_set_widget_color(unsigned long win_id, const char *widget_id, const char color[32]) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    snprintf(req.request, sizeof(req.request), "SET_WIDGET_COLOR:%s:%s", widget_id, color);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("  EE: (flux_api.c) flux_set_widget_color() -> failed to set color for widget %s\n", widget_id);

        return 1;
    }

    return 0;
}

int flux_set_widget_text(unsigned long win_id, const char *widget_id, const char *text) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    snprintf(req.request, sizeof(req.request), "SET_WIDGET_TEXT:%s:%s", widget_id, text);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("  EE: (flux_api.c) flux_set_widget_text() -> failed to set text for widget %s\n", widget_id);

        return 1;
    }

    return 0;
}

int flux_set_widget_image(unsigned long win_id, const char *widget_id, const char *filename) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    snprintf(req.request, sizeof(req.request), "SET_WIDGET_IMAGE:%s:%s", widget_id, filename);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("  EE: (flux_api.c) flux_set_widget_image() -> failed to set image for widget %s\n", widget_id);

        return 1;
    }

    return 0;
}

int flux_set_widget_font(unsigned long win_id, const char *widget_id, const char *filename, int font_size) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    snprintf(req.request, sizeof(req.request), "SET_WIDGET_FONT:%s:%s:%d", widget_id, filename, font_size);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("  EE: (flux_api.c) flux_set_widget_font() -> failed to set font for widget %s\n", widget_id);

        return 1;
    }

    return 0;
}

int flux_remove_widget(unsigned long win_id, const char *widget_id) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    snprintf(req.request, sizeof(req.request), "REMOVE_WIDGET:%s", widget_id);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("  EE: (flux_api.c) flux_remoe_widget() -> failed to remove widget %s from window %lu\n", widget_id, win_id);

        return 1;
    }

    return 0;
}

int flux_get_screen_size(unsigned long win_id, int *width, int *height) {
    WindowRequest req;
    struct {
        int w;
        int h;
    } response;

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    snprintf(req.request, sizeof(req.request), "GET_SCREEN_SIZE");
    
    if (send_request(&req, &response, sizeof(response)) < 0) {
        printf("  EE: (flux_api.c) flux_get_screen_size() -> failed to get screen size\n");

        *width = -1;
        *height = -1;

        return 1;
    }

    *width = response.w;
    *height = response.h;

    return 0;
}