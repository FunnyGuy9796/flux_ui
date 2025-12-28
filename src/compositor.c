#include "compositor.h"
#include "lib/flux_ui.h"
#include "sys_ui.h"
#include "input.h"
#include "../api/flux_api.h"
#include <fcntl.h>

typedef struct Window window_t;

int drm_fd = -1;
drmModeRes *resources = NULL;
drmModeConnector *connector = NULL;
drmModeModeInfo *mode = NULL;
drmModeEncoder *encoder = NULL;
uint32_t crtc_id = -1;
struct gbm_device *gbm = NULL;
struct gbm_surface *gbm_surface = NULL;
EGLDisplay egl_display = NULL;
EGLConfig egl_config = NULL;
EGLContext egl_context = NULL;
EGLSurface egl_surface = NULL;

GLuint program;
GLint attr_pos;
GLint uni_color;
GLint uni_rect_pos;
GLint uni_rect_size;
GLint uni_screen_size;
GLint uni_radius;
GLint uni_use_texture;
GLint uni_tex;

GLuint text_program;
GLint text_attr_pos;
GLint text_attr_uv;
GLint text_uni_glyph_pos;
GLint text_uni_glyph_size;
GLint text_uni_screen_size;
GLint text_uni_color;
GLint text_uni_tex;

GLuint comp_program;
GLint comp_attr_pos;
GLint comp_uni_tex;

GLuint vbo;

static struct gbm_bo *previous_bo = NULL;
static uint32_t previous_fb = 0;
static drmModeCrtc *orig_crtc = NULL;
static volatile sig_atomic_t running = 1;
static int frame_pending = 0;
static bool menu_open = false;

typedef struct {
    unsigned long id;
    window_t *window;
} WindowEntry;

static int server_fd;
static struct sockaddr_un addr;

static WindowEntry window_registry[MAX_WINDOWS];
static int registry_count = 0;

static window_t *requested_window;
static window_t *focused_window;
static window_t *sys_ui_win;
static window_t *sys_ui_menu_win;
static window_t *mouse_win;

static widget_t *mouse_cursor;

static int active_clients[MAX_CLIENTS];
static int client_count = 0;

static const float comp_quad[] = {
    -1.0f, -1.0f,
    1.0f, -1.0f,
    -1.0f,  1.0f,
    1.0f,  1.0f,
};

static const char *vertex_shader_src =
    "attribute vec2 pos;\n"

    "uniform mediump vec2 rect_pos;\n"
    "uniform mediump vec2 rect_size;\n"
    "uniform mediump vec2 screen_size;\n"
    "varying vec2 v_uv;\n"

    "void main() {\n"
    "   vec2 pixel = rect_pos + pos * rect_size;\n"
    "   pixel.y = screen_size.y - pixel.y;\n"
    "   vec2 ndc = (pixel / screen_size) * 2.0 - 1.0;\n"
    "   gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "   v_uv = pos;\n"
    "}\n";

static const char *fragment_shader_src =
    "#extension GL_OES_standard_derivatives : enable\n"
    "precision mediump float;\n"

    "uniform bool use_texture;\n"

    "uniform sampler2D tex;\n"
    "uniform vec4 color;\n"
    "uniform float radius;\n"
    "uniform vec2 rect_size;\n"

    "varying vec2 v_uv;\n"

    "float sdRoundRect(vec2 p, vec2 size, float r) {\n"
    "	vec2 q = abs(p - size * 0.5) - (size * 0.5 - vec2(r));\n"
    "	return length(max(q, 0.0)) - r;\n"
    "}\n"

    "void main() {\n"
    "	vec2 p = v_uv * rect_size;\n"
    "	float dist = sdRoundRect(p, rect_size, radius);\n"

    "   float aa = fwidth(dist);\n"
    "   float alpha;\n"

    "   alpha = 1.0 - smoothstep(0.0, aa, dist);\n"

    "	vec4 out_color = color;\n"
    "	out_color.a *= alpha;\n"

    "	if (use_texture) {\n"
    "		vec4 tex_color = texture2D(tex, v_uv);\n"
    "       out_color = vec4(tex_color.rgb, out_color.a * tex_color.a);\n"
    "	}\n"

    "	gl_FragColor = out_color;\n"
    "}\n";

static const char *text_vertex_shader_src =
    "attribute vec2 pos;\n"
    "attribute vec2 uv;\n"
    
    "uniform mediump vec2 glyph_pos;\n"
    "uniform mediump vec2 glyph_size;\n"
    "uniform mediump vec2 screen_size;\n"

    "varying mediump vec2 v_uv;\n"

    "void main() {\n"
    "   vec2 pixel = glyph_pos + pos * glyph_size;\n"
    "   pixel.y = screen_size.y - pixel.y;\n"

    "   vec2 ndc = (pixel / screen_size) * 2.0 - 1.0;\n"
    "   gl_Position = vec4(ndc, 0.0, 1.0);\n"

    "   v_uv = uv;\n"
    "}\n";

static const char *text_fragment_shader_src =
    "precision mediump float;\n"

    "uniform sampler2D tex;\n"
    "uniform vec4 color;\n"

    "varying mediump vec2 v_uv;\n"

    "void main() {\n"
    "   float a = texture2D(tex, v_uv).a;\n"

    "   if (a <= 0.01) {\n"
    "       discard;\n"
    "   }\n"
    
    "   gl_FragColor = vec4(color.rgb, color.a * a);\n"
    "}\n";

static const char *comp_vertex_shader_src =
    "attribute vec2 a_pos;\n"
    "varying vec2 v_uv;\n"

    "void main() {\n"
    "    v_uv = a_pos * 0.5 + 0.5;\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

static const char *comp_fragment_shader_src =
    "precision mediump float;\n"

    "uniform sampler2D u_tex;\n"
    "varying vec2 v_uv;\n"

    "void main() {\n"
    "    gl_FragColor = texture2D(u_tex, v_uv);\n"
    "}\n";

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

static void fb_destroy_callback(struct gbm_bo *bo, void *data) {
    uint32_t fb_id = (uint32_t)(uintptr_t)data;

    if (fb_id)
        drmModeRmFB(drm_fd, fb_id);
}

static uint32_t get_fb_for_bo(struct gbm_bo *bo) {
    uint32_t fb_id = (uint32_t)(uintptr_t)gbm_bo_get_user_data(bo);
    if (fb_id)
        return fb_id;

    uint32_t handles[4] = { gbm_bo_get_handle(bo).u32 };
    uint32_t strides[4] = { gbm_bo_get_stride(bo) };
    uint32_t offsets[4] = { 0 };

    uint32_t width  = gbm_bo_get_width(bo);
    uint32_t height = gbm_bo_get_height(bo);

    int ret = drmModeAddFB2(
        drm_fd,
        width,
        height,
        DRM_FORMAT_ARGB8888,
        handles,
        strides,
        offsets,
        &fb_id,
        0
    );

    if (ret) {
        printf("EE: (compositor.c) get_fb_for_bo() -> drmModeAddFB2 failed: %s\n", strerror(errno));

        return 0;
    }

    gbm_bo_set_user_data(bo, (void *)(uintptr_t)fb_id, fb_destroy_callback);

    return fb_id;
}

static void page_flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data) {
    int *waiting = (int *)data;
    *waiting = 0;
}

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint ok;

    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (!ok) {
        char log[512];

        glGetShaderInfoLog(shader, sizeof(log), NULL, log);

        printf("  EE: (compositor.c) compile_shader() -> %s\n", log);

        glDeleteShader(shader);

        return 0;
    }

    return shader;
}

static GLuint create_program() {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok;

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok) {
        char log[512];

        glGetProgramInfoLog(prog, sizeof(log), NULL, log);

        printf("  EE: (compositor.c) create_program() -> %s\n", log);

        glDeleteProgram(prog);

        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}

static GLuint create_text_program() {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, text_vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, text_fragment_shader_src);
    
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok;

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok) {
        char log[512];

        glGetProgramInfoLog(prog, sizeof(log), NULL, log);

        printf("  EE: (compositor.c) create_text_program() -> %s\n", log);

        glDeleteProgram(prog);

        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}

static GLuint create_comp_program() {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, comp_vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, comp_fragment_shader_src);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok;

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok) {
        char log[512];

        glGetProgramInfoLog(prog, sizeof(log), NULL, log);

        printf("  EE: (compositor.c) create_comp_program() -> %s\n", log);

        glDeleteProgram(prog);

        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}

void cleanup() {
    printf("  II: (compositor.c) cleanup() -> cleaning up...\n");

    if (previous_bo) {
        if (previous_fb)
            drmModeRmFB(drm_fd, previous_fb);

        gbm_surface_release_buffer(gbm_surface, previous_bo);

        previous_bo = NULL;
        previous_fb = 0;
    }

    if (orig_crtc) {
        drmModeSetCrtc(
            drm_fd,
            orig_crtc->crtc_id,
            orig_crtc->buffer_id,
            orig_crtc->x,
            orig_crtc->y,
            &connector->connector_id,
            1,
            &orig_crtc->mode
        );

        drmModeFreeCrtc(orig_crtc);

        orig_crtc = NULL;
    }

    if (egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl_display,
                       EGL_NO_SURFACE,
                       EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);

        if (egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(egl_display, egl_surface);

            egl_surface = EGL_NO_SURFACE;
        }

        if (egl_context != EGL_NO_CONTEXT) {
            eglDestroyContext(egl_display, egl_context);

            egl_context = EGL_NO_CONTEXT;
        }

        eglTerminate(egl_display);

        egl_display = EGL_NO_DISPLAY;
    }
    
    if (gbm_surface) {
        gbm_surface_destroy(gbm_surface);

        gbm_surface = NULL;
    }

    if (gbm) {
        gbm_device_destroy(gbm);

        gbm = NULL;
    }

    if (connector) {
        drmModeFreeConnector(connector);

        connector = NULL;
    }

    if (encoder) {
        drmModeFreeEncoder(encoder);

        encoder = NULL;
    }

    if (resources) {
        drmModeFreeResources(resources);

        resources = NULL;
    }

    if (drm_fd >= 0)
        drmDropMaster(drm_fd);

    if (drm_fd >= 0) {
        close(drm_fd);

        drm_fd = -1;
    }

    if (server_fd >= 0) {
        close(server_fd);
        unlink(SOCKET_PATH);

        server_fd = -1;
    }
}

int init() {
    drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);

    if (drm_fd == -1) {
        printf("  EE: (compositor.c) init() -> failed to open '/dev/dri/card0'\n  %s\n", strerror(errno));

        return 1;
    }

    printf("  II: (compositor.c) init() -> video card... [OK]\n");
    
    if (drmSetMaster(drm_fd) != 0) {
        printf("  WW: (compositor.c) init() -> failed to become DRM master: %s\n", strerror(errno));

        return 1;
    }
    
    printf("  II: (compositor.c) init() -> DRM master... [OK]\n");

    resources = drmModeGetResources(drm_fd);
    
    if (!resources) {
        printf("  EE: (compositor.c) init() -> drmModeGetResources failed\n");

        return 1;
    }
    
    for (int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(drm_fd, resources->connectors[i]);

        if (conn->connection == DRM_MODE_CONNECTED) {
            connector = conn;

            break;
        }

        drmModeFreeConnector(conn);
    }

    if (!connector) {
        printf("  EE: (compositor.c) init() -> failed to connect to the display\n");

        return 1;
    }

    printf("  II: (compositor.c) init() -> display connector... [OK]\n");

    mode = &connector->modes[0];

    printf("  II: (compositor.c) init() -> display mode: %dx%d @%dHz... [OK]\n", mode->hdisplay, mode->vdisplay, mode->vrefresh);

    encoder = drmModeGetEncoder(drm_fd, connector->encoder_id);

    if (!encoder) {
        printf("  EE: (compositor.c) init() -> no encoder found\n");

        return 1;
    }

    crtc_id = encoder->crtc_id;

    printf("  II: (compositor.c) init() -> CRTC ID: %u... [OK]\n", crtc_id);

    orig_crtc = drmModeGetCrtc(drm_fd, crtc_id);

    if (!orig_crtc) {
        printf("  EE: (compositor.c) init() -> failed to save original CRTC\n");

        return 1;
    }

    gbm = gbm_create_device(drm_fd);

    if (!gbm) {
        printf("  EE: (compositor.c) init() -> failed to create GBM device\n");

        return 1;
    }

    printf("  II: (compositor.c) init() -> GBM device... [OK]\n");

    gbm_surface = gbm_surface_create(gbm,
        mode->hdisplay,
        mode->vdisplay,
        GBM_FORMAT_ARGB8888,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING
    );
    
    if (!gbm_surface) {
        printf("  EE: (compositor.c) init() -> failed to create GBM surface with ARGB8888\n");

        return 1;
    }
    
    printf("  II: (compositor.c) init() -> GBM surface (ARGB8888)... [OK]\n");

    egl_display = eglGetDisplay((EGLNativeDisplayType)gbm);

    if (egl_display == EGL_NO_DISPLAY) {
        printf("  EE: (compositor.c) init() -> eglGetDisplay failed\n");

        return 1;
    }

    if (!eglInitialize(egl_display, NULL, NULL)) {
        printf("  EE: (compositor.c) init() -> eglInitialize failed\n");

        return 1;
    }

    printf("  II: (compositor.c) init() -> EGL display... [OK]\n");
    
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        printf("  EE: (compositor.c) init() -> eglBindAPI failed (error: 0x%x)\n", eglGetError());

        return 1;
    }
    
    EGLint num_configs = 0;
    EGLConfig configs[128];
    
    EGLint config_attrs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    if (!eglChooseConfig(egl_display, config_attrs, configs, 128, &num_configs) || num_configs < 1) {
        printf("  EE: (compositor.c) init() -> eglChooseConfig failed (error: 0x%x)\n", eglGetError());

        return 1;
    }
    
    uint32_t gbm_format = GBM_FORMAT_ARGB8888;
    
    egl_config = NULL;

    for (int i = 0; i < num_configs; i++) {
        EGLint visual_id;

        if (eglGetConfigAttrib(egl_display, configs[i], EGL_NATIVE_VISUAL_ID, &visual_id)) {
            if (visual_id == (EGLint)gbm_format) {
                egl_config = configs[i];

                break;
            }
        }
    }
    
    if (!egl_config) {
        EGLint alt_attrs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NATIVE_VISUAL_ID, gbm_format,
            EGL_NONE
        };
        
        if (eglChooseConfig(egl_display, alt_attrs, &egl_config, 1, &num_configs) && num_configs > 0) {
            EGLint visual_id;

            eglGetConfigAttrib(egl_display, egl_config, EGL_NATIVE_VISUAL_ID, &visual_id);
        } else {
            printf("  EE: (compositor.c) init() -> could not find matching EGL config\n");

            egl_config = configs[0];
        }
    }

    printf("  II: (compositor.c) init() -> EGL config... [OK]\n");

    EGLint context_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attrs);

    if (egl_context == EGL_NO_CONTEXT) {
        printf("  EE: (compositor.c) init() -> eglCreateContext failed (error: 0x%x)\n", eglGetError());

        return 1;
    }

    printf("  II: (compositor.c) init() -> EGL context... [OK]\n");
    
    EGLint red, green, blue, alpha, native_visual;

    eglGetConfigAttrib(egl_display, egl_config, EGL_RED_SIZE, &red);
    eglGetConfigAttrib(egl_display, egl_config, EGL_GREEN_SIZE, &green);
    eglGetConfigAttrib(egl_display, egl_config, EGL_BLUE_SIZE, &blue);
    eglGetConfigAttrib(egl_display, egl_config, EGL_ALPHA_SIZE, &alpha);
    eglGetConfigAttrib(egl_display, egl_config, EGL_NATIVE_VISUAL_ID, &native_visual);

    EGLint surface_attrs[] = {
        EGL_NONE
    };
    
    egl_surface = eglCreateWindowSurface(egl_display, egl_config, (EGLNativeWindowType)gbm_surface, surface_attrs);

    if (egl_surface == EGL_NO_SURFACE) {
        EGLint error = eglGetError();

        printf("  EE: (compositor.c) init() -> eglCreateWindowSurface failed\n");
        
        switch (error) {
            case EGL_BAD_MATCH:
                printf("    (EGL_BAD_MATCH - config/window mismatch)\n");

                break;
            case EGL_BAD_CONFIG:
                printf("    (EGL_BAD_CONFIG - invalid config)\n");

                break;
            case EGL_BAD_NATIVE_WINDOW:
                printf("    (EGL_BAD_NATIVE_WINDOW - invalid window)\n");

                break;
            case EGL_BAD_ALLOC:
                printf("    (EGL_BAD_ALLOC - allocation failed)\n");

                break;
            default:
                printf("    (unknown error)\n");

                break;
        }
        
        PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC eglCreatePlatformWindowSurfaceEXT = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
        
        if (eglCreatePlatformWindowSurfaceEXT) {
            egl_surface = eglCreatePlatformWindowSurfaceEXT(
                egl_display, 
                egl_config, 
                gbm_surface,
                NULL
            );
            
            if (egl_surface == EGL_NO_SURFACE) {
                error = eglGetError();

                printf("  EE: (compositor.c) init() -> eglCreatePlatformWindowSurfaceEXT also failed (error: 0x%x)\n", error);

                return 1;
            }
        } else {
            printf("  EE: (compositor.c) init() -> eglCreatePlatformWindowSurfaceEXT not available\n");
            
            EGLint alt_config_attrs[] = {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 0,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_NONE
            };
            EGLConfig alt_config;

            if (eglChooseConfig(egl_display, alt_config_attrs, &alt_config, 1, &num_configs) && num_configs > 0) {
                egl_surface = eglCreateWindowSurface(egl_display, alt_config, (EGLNativeWindowType)gbm_surface, surface_attrs);
                
                if (egl_surface == EGL_NO_SURFACE) {
                    printf("  EE: (compositor.c) init() -> alternative config also failed (error: 0x%x)\n", eglGetError());

                    return 1;
                }
                
                egl_config = alt_config;
            } else {
                printf("  EE: (compositor.c) init() -> could not find alternative config\n");

                return 1;
            }
        }
    }

    if (!eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context)) {
        printf("  EE: (compositor.c) init() -> eglMakeCurrent failed\n");

        return 1;
    }

    printf("  II: (compositor.c) init() -> EGL surface... [OK]\n");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    program = create_program();

    if (!program) {
        printf("  EE: (compositor.c) init() -> failed to create main shader program\n");

        return 1;
    }

    glUseProgram(program);

    attr_pos = glGetAttribLocation(program, "pos");
    uni_color = glGetUniformLocation(program, "color");
    uni_rect_pos = glGetUniformLocation(program, "rect_pos");
    uni_rect_size = glGetUniformLocation(program, "rect_size");
    uni_screen_size = glGetUniformLocation(program, "screen_size");
    uni_radius = glGetUniformLocation(program, "radius");
    uni_use_texture = glGetUniformLocation(program, "use_texture");
    uni_tex = glGetUniformLocation(program, "tex");

    text_program = create_text_program();

    if (!text_program) {
        printf("  EE: (compositor.c) init() -> failed to create text shader program\n");

        return 1;
    }

    text_attr_pos = glGetAttribLocation(text_program, "pos");
    text_attr_uv = glGetAttribLocation(text_program, "uv");
    text_uni_glyph_pos = glGetUniformLocation(text_program, "glyph_pos");
    text_uni_glyph_size = glGetUniformLocation(text_program, "glyph_size");
    text_uni_screen_size = glGetUniformLocation(text_program, "screen_size");
    text_uni_color = glGetUniformLocation(text_program, "color");
    text_uni_tex = glGetUniformLocation(text_program, "tex");

    comp_program = create_comp_program();

    if (!comp_program) {
        printf("  EE: (compositor.c) init() -> failed to create compositor shader program\n");

        return 1;
    }

    comp_attr_pos = glGetAttribLocation(comp_program, "a_pos");
    comp_uni_tex = glGetAttribLocation(comp_program, "u_tex");

    glGenBuffers(1, &vbo);

    return 0;
}

int render_frame() {
    GLenum err = glGetError();
    
    if (err != GL_NO_ERROR) {
        printf("  EE: (compositor.c) render_frame() -> OpenGL error after drawing frame: 0x%x\n", err);

        return 1;
    }
    
    if (!eglSwapBuffers(egl_display, egl_surface)) {
        printf("  EE: (compositor.c) render_frame() -> eglSwapBuffers failed (error: 0x%x)\n", eglGetError());

        return 1;
    }
    
    struct gbm_bo *bo = gbm_surface_lock_front_buffer(gbm_surface);
    
    if (!bo) {
        printf("  EE: (compositor.c) render_frame() -> gbm_surface_lock_front_buffer failed\n");

        return 1;
    }
    
    uint32_t fb_id = get_fb_for_bo(bo);
    
    if (!fb_id) {
        gbm_surface_release_buffer(gbm_surface, bo);

        return 1;
    }
    
    if (!previous_bo) {
        int ret = drmModeSetCrtc(drm_fd, crtc_id, fb_id, 0, 0, &connector->connector_id, 1, mode);
        
        if (ret) {
            printf("  EE: (compositor.c) render_frame() -> drmModeSetCrtc failed: %s\n", strerror(errno));
            gbm_surface_release_buffer(gbm_surface, bo);

            return 1;
        }
    } else {
        frame_pending = 1;

        int ret = drmModePageFlip(
            drm_fd,
            crtc_id,
            fb_id,
            DRM_MODE_PAGE_FLIP_EVENT,
            &frame_pending);
        
        if (ret) {
            printf("  EE: (compositor.c) render_frame() -> drmModePageFlip failed: %s\n", strerror(errno));
            gbm_surface_release_buffer(gbm_surface, bo);

            return 1;
        }
        
        drmEventContext ev = {};
        ev.version = DRM_EVENT_CONTEXT_VERSION;
        ev.page_flip_handler = page_flip_handler;
        
        struct pollfd fds = {
            .fd = drm_fd,
            .events = POLLIN,
        };
        
        int timeout_count = 0;

        while (frame_pending && timeout_count < 10) {
            ret = poll(&fds, 1, 100);
            
            if (ret < 0) {
                printf("  EE: (compositor.c) render_frame() -> poll failed: %s\n", strerror(errno));

                break;
            } else if (ret == 0) {
                printf("  WW: (compositor.c) render_frame() -> poll timeout waiting for page flip (attempt %d/10)\n", ++timeout_count);

                continue;
            }
            
            if (fds.revents & POLLIN) {
                ret = drmHandleEvent(drm_fd, &ev);

                if (ret) {
                    printf("  EE: (compositor.c) render_frame() -> drmHandleEvent failed: %s\n", strerror(errno));
                    
                    break;
                }
            }
        }
        
        if (frame_pending) {
            printf("  EE: (compositor.c) render_frame() -> page flip never completed after 10 timeouts\n");
            
            return 1;
        }
        
        gbm_surface_release_buffer(gbm_surface, previous_bo);
    }
    
    previous_bo = bo;
    previous_fb = fb_id;
    
    return 0;
}

void comp_draw_texture(GLuint tex) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, mode->hdisplay, mode->vdisplay);

    glUseProgram(comp_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(comp_uni_tex, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(comp_quad), comp_quad, GL_STATIC_DRAW);

    glEnableVertexAttribArray(comp_attr_pos);
    glVertexAttribPointer(comp_attr_pos, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void comp_redraw(window_t *window, float dt) {
    if (!window) {
        printf("  EE: (compositor.c) comp_redraw() -> attempted to redraw invalid window\n");
        
        cleanup();
        exit(1);
    }

    if (!ui_window_get_rendered(window))
        return;

    ui_call_render_loop(window, dt);
    ui_render_window(window);

    GLuint window_tex = ui_window_get_texture(window);

    comp_draw_texture(window_tex);
}

int comp_create_socket() {
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd < 0) {
        printf("  EE: (compositor.c) comp_create_socket() -> failed to create server_fd\n");

        return 1;
    }

    int flags = fcntl(server_fd, F_GETFL, 0);

    if (flags == -1) {
        printf("  EE: (compositor.c) comp_create_socket() -> fcntl F_GETFL failed\n");

        return 1;
    }

    if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        printf("  EE: (compositor.c) comp_create_socket() -> fcntl F_SETFL failed\n");

        return 1;
    }

    unlink(SOCKET_PATH);

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("  EE: (compositor.c) comp_create_socket() -> failed to bind server socket\n");

        return 1;
    }

    if (listen(server_fd, 5) < 0) {
        printf("  EE: (compositor.c) comp_create_socket() -> server failed to listen\n");

        return 1;
    }

    printf("  II: (compositor.c) init() -> API socket... [OK]\n");

    return 0;
}

unsigned long comp_register_window(window_t *window) {
    if (!window) {
        printf("  EE: (compositor.c) comp_register_window() -> invalid window\n");

        return 0;
    }

    if (registry_count >= MAX_WINDOWS) {
        printf("  EE: (compositor.c) comp_register_window() -> window registry full\n");

        return 0;
    }

    unsigned long id = ui_window_get_id(window);

    window_registry[registry_count].id = id;
    window_registry[registry_count].window = window;
    registry_count++;

    return id;
}

void comp_remove_window(window_t *window) {
    if (!window) {
        printf("  EE: (compositor.c) comp_remove_window() -> invalid window\n");

        return;
    }

    if (registry_count == 0) {
        printf("  EE: (compositor.c) comp_remove_window() -> window registry empty\n");

        return;
    }

    unsigned long id = ui_window_get_id(window);

    ui_destroy_window(window);

    for (int i = 0; i < registry_count; i++) {
        if (window_registry[i].id == id) {
            window_registry[i] = window_registry[registry_count];
            registry_count--;

            return;
        }
    }

    printf("  WW: (compositor.c) comp_remove_window() -> window %lu not found\n", id);
}

void comp_handle_widget_command(window_t *window, const char *command) {
    char widget_id[64];
    float x, y, w, h;
    int radius;
    char color[32];
    char text[256];
    char font_file[64];
    int font_size;
    widget_type_t widg_type;

    if (sscanf(command, "CREATE_WIDGET:%63[^:]:%u", widget_id, &widg_type) == 2) {
        widget_t *widget = ui_create_widget(widget_id, widg_type);

        ui_append_widget(window, widget);
    } else if (sscanf(command, "SET_WIDGET_GEOMETRY:%63[^:]:%f:%f:%f:%f:%d", widget_id, &x, &y, &w, &h, &radius) == 6) {
        widget_t *widget = ui_window_get_widget(window, widget_id);

        if (!widget) {
            printf("  WW: (compositor.c) comp_handle_widget_command() -> SET_WIDGET_GEOMETRY on invalid widget\n");

            return;
        }

        ui_widget_set_geometry(widget, x, y, w, h, radius);
    } else if (sscanf(command, "SET_WIDGET_COLOR:%63[^:]:%31s", widget_id, color) == 2) {
        widget_t *widget = ui_window_get_widget(window, widget_id);

        if (!widget) {
            printf("  WW: (compositor.c) comp_handle_widget_command() -> SET_WIDGET_COLOR on invalid widget\n");

            return;
        }

        ui_widget_set_color(widget, color);
    } else if (sscanf(command, "SET_WIDGET_TEXT:%63[^:]:%255s", widget_id, text) == 2) {
        widget_t *widget = ui_window_get_widget(window, widget_id);

        if (!widget) {
            printf("  WW: (compositor.c) comp_handle_widget_command() -> SET_WIDGET_TEXT on invalid widget\n");

            return;
        }

        ui_widget_set_text(widget, text);
    } else if (sscanf(command, "SET_WIDGET_FONT:%63[^:]:%63s:%d", widget_id, font_file, &font_size) == 3) {
        widget_t *widget = ui_window_get_widget(window, widget_id);

        if (!widget) {
            printf("  WW: (compositor.c) comp_handle_widget_command() -> LOAD_WIDGET_FONT on invalid widget\n");

            return;
        }

        font_t *font = ui_load_font(font_file, font_size);

        ui_widget_set_font(widget, font);
    } else if (sscanf(command, "REMOVE_WIDGET:%63s", widget_id) == 1) {
        widget_t *widget = ui_window_get_widget(window, widget_id);

        if (!widget) {
            printf("  WW: (compositor.c) comp_handle_widget_command() -> REMOVE_WIDGET on invalid widget\n");

            return;
        }

        ui_remove_widget(window, widget);
    }
}

window_t *comp_get_window(unsigned long id) {
    for (int i = 0; i < registry_count; i++) {
        if (window_registry[i].id == id)
            return window_registry[i].window;
    }

    return NULL;
}

void comp_listen_socket() {
    int new_client = accept(server_fd, NULL, NULL);

    if (new_client >= 0) {
        if (client_count < MAX_CLIENTS) {
            int flags = fcntl(new_client, F_GETFL, 0);

            fcntl(new_client, F_SETFL, flags | O_NONBLOCK);
            
            active_clients[client_count++] = new_client;

            printf("  II: comp_listen_socket() -> new client connected (fd: %d)\n", new_client);
        } else
            close(new_client);
    }

    for (int i = 0; i < client_count; i++) {
        WindowRequest request;
        ssize_t bytes = recv(active_clients[i], &request, sizeof(request), 0);

        if (bytes == sizeof(request)) {
            printf("  II: (compositor.c) comp_listen_socket() -> request received: %s\n", request.request);

            if (strcmp(request.request, "CREATE_WINDOW") == 0) {
                window_t *new_win = ui_create_window();

                unsigned long id = comp_register_window(new_win);

                send(active_clients[i], &id, sizeof(id), 0);
            } else {
                window_t *window = comp_get_window(request.id);

                if (window) {
                    if (strcmp(request.request, "RENDER") == 0)
                        requested_window = window;
                    else if (strcmp(request.request, "SHOW") == 0)
                        ui_request_render(window);
                    else if (strcmp(request.request, "HIDE") == 0)
                        ui_request_hide(window);
                    else if (strcmp(request.request, "DESTROY") == 0) {
                        comp_remove_window(window);

                        requested_window = window_registry[registry_count].window;
                    } else if (strncmp(request.request, "CREATE_WIDGET:", 14) == 0)
                        comp_handle_widget_command(window, request.request);
                    else if (strncmp(request.request, "SET_WIDGET_GEOMETRY:", 20) == 0)
                        comp_handle_widget_command(window, request.request);
                    else if (strncmp(request.request, "SET_WIDGET_COLOR:", 17) == 0)
                        comp_handle_widget_command(window, request.request);
                    else if (strncmp(request.request, "SET_WIDGET_TEXT:", 16) == 0)
                        comp_handle_widget_command(window, request.request);
                    else if (strncmp(request.request, "LOAD_WIDGET_FONT:", 17) == 0)
                        comp_handle_widget_command(window, request.request);
                    else if (strncmp(request.request, "REMOVE_WIDGET:", 14) == 0)
                        comp_handle_widget_command(window, request.request);
                    else {
                        printf("  EE: (compositor.c) comp_listen_socket() -> invalid command from window ID %lu\n", request.id);

                        const char *err = "ERROR: invalid command";

                        send(active_clients[i], err, strlen(err), 0);

                        continue;
                    }

                    send(active_clients[i], "OK", 2, 0);
                } else {
                    printf("  EE: (compositor.c) comp_listen_socket() -> window ID %lu not found\n", request.id);

                    const char *err = "ERROR: invalid window";

                    send(active_clients[i], err, strlen(err), 0);
                }
            }
        } else if (bytes == 0) {
            printf("  II: (compositor.c) comp_listen_socket() -> client disconnected (fd: %d)\n", active_clients[i]);

            close(active_clients[i]);

            active_clients[i] = active_clients[--client_count];
            i--;
        }
    }
}

void comp_on_mouse_move(int x, int y) {
    ui_widget_set_geometry(mouse_cursor, x, y, -1, -1, -1);
}

void comp_on_mouse_down(int x, int y, uint32_t button) {
    
}

void comp_on_mouse_up(int x, int y, uint32_t button) {
    
}

void comp_on_scroll(int dx, int dy) {
    
}

void comp_on_key_down(uint32_t key, uint32_t mods) {
    switch (key) {
        case 125: {
            if (menu_open) {
                ui_request_hide(sys_ui_menu_win);

                if (requested_window)
                    focused_window = requested_window;
                else
                    focused_window = sys_ui_win;
            } else {
                ui_request_render(sys_ui_menu_win);

                focused_window = sys_ui_menu_win;
            }

            menu_open = !menu_open;

            return;
        }
    }

    printf("  II: (compositor.c) comp_on_key_down() -> key pressed: %d\n", key);
}

void comp_on_key_up(uint32_t key, uint32_t mods) {
    
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);

    printf("---------- FluxUI ----------\n");
    printf("  II: (compositor.c) main() -> initializing...\n");

    int init_status = init();

    if (init_status != 0) {
        printf("  EE: (compositor.c) main() -> an error occurred in init()\n");

        running = false;
    }

    int input_status = input_init();

    if (input_status != 0) {
        printf("  EE: (compositor.c) main() -> an error occurred in input_init()\n");

        running = false;
    }

    int socket_status = comp_create_socket();

    if (socket_status != 0) {
        printf("  EE: (compositor.c) main() -> an error occurred in comp_create_socket()\n");

        running = false;
    }

    sys_ui_win = sys_ui_init();
    sys_ui_menu_win = sys_ui_menu();

    mouse_win = ui_create_window();
    mouse_cursor = ui_create_widget("sys-cursor", WIDGET_IMAGE);

    GLuint cursor_image = ui_load_texture("assets/cursors/default.png");

    ui_widget_set_geometry(mouse_cursor, mode->hdisplay / 2.0, mode->vdisplay / 2.0, 24, 24, -1);
    ui_widget_set_color(mouse_cursor, "#ffffffff");
    ui_widget_set_image(mouse_cursor, cursor_image);
    ui_append_widget(mouse_win, mouse_cursor);
    ui_request_render(mouse_win);

    struct timespec last_time;

    clock_gettime(CLOCK_MONOTONIC, &last_time);

    struct pollfd fds[2];
    
    fds[0].fd = drm_fd;
    fds[0].events = POLLIN;
    fds[1].fd = input_get_fd();
    fds[1].events = POLLIN;

    long unsigned int frame_count = 0;
    bool opened = false;

    while (running) {
        int poll_ret = poll(fds, 2, 16);

        if (poll_ret < 0) {
            if (errno == EINTR)
                continue;

            printf("  EE: (compositor.c) main() -> poll failed\n");

            break;
        }

        if (fds[1].revents & POLLIN)
            input_process_event();

        if (fds[0].revents & POLLIN) {
            drmEventContext ev = {
                .version = DRM_EVENT_CONTEXT_VERSION,
                .page_flip_handler = page_flip_handler
            };

            drmHandleEvent(drm_fd, &ev);
        }

        comp_listen_socket();

        struct timespec now;

        clock_gettime(CLOCK_MONOTONIC, &now);

        float dt = (now.tv_sec - last_time.tv_sec) + (now.tv_nsec - last_time.tv_nsec) / 1e9f;

        last_time = now;

        if (frame_count > 10 && !opened) {
            system("./test &");

            opened = true;
        }

        if (!frame_pending) {
            if (!focused_window) {
                registry_count = 0;
                menu_open = false;
                requested_window = NULL;
                focused_window = sys_ui_win;
            }

            if (requested_window)
                comp_redraw(requested_window, dt);
            else
                comp_redraw(sys_ui_win, dt);

            if (menu_open)
                comp_redraw(sys_ui_menu_win, dt);

            comp_redraw(mouse_win, dt);

            int ret = render_frame();
        
            if (ret != 0) {
                printf("\n  EE: (compositor.c) main() -> render_frame failed\n");
                
                running = false;
            }

            frame_count++;
        }
    }
    
    cleanup();
    input_cleanup();

    printf("  II: (compositor.c) main() -> finished\n");

    return 0;
}