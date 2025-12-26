#include "sys_ui.h"
#include "compositor.h"

static font_t *ui_font_body;
static font_t *ui_font_heading;
static GLuint ui_game_image;

static window_t *sys_window;
static widget_t *sys_background;
static widget_t *sys_clock;
static widget_t *sys_recent_game;

static window_t *menu_window;
static widget_t *menu_background;
static widget_t *menu_body;
static widget_t *menu_clock;

static float curr_tick = 0;
static float past_x = 0;
static float curr_x = 0;
static float past_y = 0;
static float curr_y = 0;
static char past_time[9];

static void set_time(widget_t *clock) {
    time_t now = time(NULL);

    if (now == (time_t)-1) {
        printf("  EE: (sys_ui.c) set_time() -> unable to get current time\n");

        exit(1);
    }

    struct tm *tm_info = localtime(&now);

    if (!tm_info) {
        printf("  EE: (sys_ui.c) set_time() -> unable to get local time\n");

        exit(1);
    }

    int hour24 = tm_info->tm_hour;
    int minutes = tm_info->tm_min;
    int hour12 = hour24 % 12;

    if (hour12 == 0)
        hour12 = 12;

    const char *apm = (hour24 >= 12) ? "pm" : "am";
    char time[9];

    sprintf(time, "%02d:%02d %s", hour12, minutes, apm);

    if (strcmp(time, past_time) != 0) {
        ui_widget_set_text(clock, time);

        float width, height;

        ui_measure_text(time, ui_font_heading, &width, &height);

        curr_x = mode->hdisplay - (width + 40);
        curr_y = height + 20;

        if (curr_x != past_x || curr_y != past_y) {
            ui_widget_set_geometry(clock, curr_x, curr_y, -1, -1, -1, -1);

            past_x = curr_x;
            past_y = curr_y;
        }

        strcpy(past_time, time);
    }
}

void menu_ui_render(window_t *window, float dt) {
    set_time(menu_clock);
}

window_t *sys_ui_menu() {
    menu_window = ui_create_window();

    menu_background = ui_create_widget("menu-background", WIDGET_RECT);

    ui_widget_set_geometry(menu_background, 0, 0, mode->hdisplay, mode->vdisplay, 0, -1);
    ui_widget_set_color(menu_background, "#000000b2");
    ui_append_widget(menu_window, menu_background);

    menu_body = ui_create_widget("menu-body", WIDGET_RECT);

    float menu_y = (mode->vdisplay - 40) - 400;

    ui_widget_set_geometry(menu_body, 40, menu_y, mode->hdisplay - 80, 400, 10, -1);
    ui_widget_set_color(menu_body, "#1c1c1cff");
    ui_append_widget(menu_window, menu_body);

    menu_clock = ui_create_widget("menu-clock", WIDGET_TEXT);

    ui_widget_set_geometry(menu_clock, 20, 40, 50, 50, -1, -1);
    ui_widget_set_color(menu_clock, "#ffffffff");
    ui_widget_set_font(menu_clock, ui_font_heading);
    ui_widget_set_text(menu_clock, "CLOCK");
    ui_append_widget(menu_window, menu_clock);

    ui_set_render_loop(menu_window, menu_ui_render);

    ui_request_render(menu_window);

    return menu_window;
}

void sys_ui_render(window_t *window, float dt) {
    set_time(sys_clock);
}

window_t *sys_ui_init() {
    ui_font_body = ui_load_font("assets/roboto.ttf", 24);
    ui_font_heading = ui_load_font("assets/roboto.ttf", 48);

    sys_window = ui_create_window();

    sys_background = ui_create_widget("sys-background", WIDGET_RECT);

    ui_widget_set_geometry(sys_background, 0, 0, mode->hdisplay, mode->vdisplay, 0, -1);
    ui_widget_set_color(sys_background, "#1c1c1cff");
    ui_append_widget(sys_window, sys_background);

    sys_clock = ui_create_widget("sys-clock", WIDGET_TEXT);

    ui_widget_set_geometry(sys_clock, 20, 40, 50, 50, -1, -1);
    ui_widget_set_color(sys_clock, "#ffffffff");
    ui_widget_set_font(sys_clock, ui_font_heading);
    ui_widget_set_text(sys_clock, "CLOCK");
    ui_append_widget(sys_window, sys_clock);

    sys_recent_game = ui_create_widget("sys-recent-game", WIDGET_IMAGE);

    ui_widget_set_geometry(sys_recent_game, 100, 500, 200, 200, 10, -1);
    ui_widget_set_color(sys_recent_game, "#ffffffff");
    
    ui_game_image = ui_load_texture("assets/test.jpg");

    if (ui_game_image == 0) {
        printf("  EE: sys_ui_init() -> failed to load ui_game_image\n");

        exit(1);
    }

    ui_widget_set_image(sys_recent_game, ui_game_image);
    ui_append_widget(sys_window, sys_recent_game);

    ui_set_render_loop(sys_window, sys_ui_render);

    ui_request_render(sys_window);

    sys_ui_menu();

    return sys_window;
}