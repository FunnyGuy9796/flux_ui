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

    strncpy(req.request, "CREATE", sizeof(req.request) - 1);

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