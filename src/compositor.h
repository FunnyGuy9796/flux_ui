#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

typedef EGLSurface (*PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)(
    EGLDisplay dpy,
    EGLConfig config,
    void *native_window,
    const EGLint *attrib_list
);

#ifndef EGL_BAD_MATCH
#define EGL_BAD_MATCH 0x3009
#endif
#ifndef EGL_BAD_CONFIG
#define EGL_BAD_CONFIG 0x3005
#endif
#ifndef EGL_BAD_NATIVE_WINDOW
#define EGL_BAD_NATIVE_WINDOW 0x300B
#endif
#ifndef EGL_BAD_ALLOC
#define EGL_BAD_ALLOC 0x3003
#endif

#define SOCKET_PATH "/tmp/flux_comp.sock"
#define MAX_WINDOWS 10

extern int drm_fd;
extern drmModeRes *resources;
extern drmModeConnector *connector;
extern drmModeModeInfo *mode;
extern drmModeEncoder *encoder;
extern uint32_t crtc_id;
extern struct gbm_device *gbm;
extern struct gbm_surface *gbm_surface;
extern EGLDisplay egl_display;
extern EGLConfig egl_config;
extern EGLContext egl_context;
extern EGLSurface egl_surface;
extern GLuint program;
extern GLint attr_pos;
extern GLint uni_color;
extern GLint uni_rect_pos;
extern GLint uni_rect_size;
extern GLint uni_screen_size;
extern GLint uni_radius;
extern GLint uni_outline;
extern GLint uni_border_width;
extern GLint uni_use_texture;
extern GLint uni_tex;
extern GLuint text_program;
extern GLint text_attr_pos;
extern GLint text_attr_uv;
extern GLint text_uni_glyph_pos;
extern GLint text_uni_glyph_size;
extern GLint text_uni_screen_size;
extern GLint text_uni_color;
extern GLint text_uni_tex;
extern GLuint vbo;

#endif