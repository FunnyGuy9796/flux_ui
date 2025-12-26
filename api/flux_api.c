#include "flux_api.h"

static int send_request(WindowRequest *req, void *response, size_t response_size) {
    int sock_fd;
    struct sockaddr_un addr;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock_fd < 0) {
        printf("EE: send_request() -> failed to connect to socket");

        return 1;
    }

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("EE: send_request() -> failed to send\n");

        close(sock_fd);

        return 1;
    }

    if (response && response_size > 0) {
        ssize_t bytes = recv(sock_fd, response, response_size, 0);

        if (bytes < 0) {
            printf("EE: send_request() -> failed to receive response\n");

            close(sock_fd);

            return 1;
        }
    }

    close(sock_fd);

    return 0;
}

unsigned long flux_create_window() {
    WindowRequest req;
    unsigned long win_id = 0;

    memset(&req, 0, sizeof(req));

    req.id = 0;

    strncpy(req.request, "CREATE_WINDOW", sizeof(req.request) - 1);

    if (send_request(&req, &win_id, sizeof(win_id)) < 0) {
        printf("EE: flux_create_window() -> failed to create window\n");

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
        printf("EE: flux_show_window() -> failed to show window %lu\n", win_id);

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
        printf("EE: flux_hide_window() -> failed to hide window %lu\n", win_id);

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
        printf("EE: flux_render_window() -> failed to render window %lu\n", win_id);

        return 1;
    }

    return 0;
}

int flux_destory_window(unsigned long win_id) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    strncpy(req.request, "DESTROY", sizeof(req.request) - 1);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("EE: flux_destroy_window() -> failed to destroy window %lu\n", win_id);

        return 1;
    }

    return 0;
}

int flux_add_widget(unsigned long win_id, const char *widget_id) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    snprintf(req.request, sizeof(req.request), "CREATE_WIDGET:%s", widget_id);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("EE: flux_add_widget() -> failed to add widget %s\n", widget_id);

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
        printf("EE: flux_set_widget_geometry() -> failed to set geometry for widget %s\n", widget_id);

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
        printf("EE: flux_set_widget_color() -> failed to set color for widget %s\n", widget_id);

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
        printf("EE: flux_set_widget_text() -> failed to set text for widget %s\n", widget_id);

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
        printf("EE: flux_set_widget_image() -> failed to set image for widget %s\n", widget_id);

        return 1;
    }

    return 0;
}

int flux_set_widget_font(unsigned long win_id, const char *widget_id, const char *filename) {
    WindowRequest req;
    char response[32];

    memset(&req, 0, sizeof(req));

    req.id = win_id;

    snprintf(req.request, sizeof(req.request), "SET_WIDGET_FONT:%s:%s", widget_id, filename);

    if (send_request(&req, response, sizeof(response)) < 0) {
        printf("EE: flux_set_widget_font() -> failed to set font for widget %s\n", widget_id);

        return 1;
    }

    return 0;
}