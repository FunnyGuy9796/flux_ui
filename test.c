#include <stdio.h>
#include "api/flux_api.h"

unsigned long win_id = -1;

int main() {
    printf("test.c: initializing API...\n");
    
    if (flux_init() != 0) {
        printf("test.c: failed to initialize API\n");
        
        return 1;
    }
    
    printf("test.c: making API call...\n");
    
    win_id = flux_create_window();

    if (win_id == 0) {
        printf("test.c: failed to create window\n");

        flux_shutdown();

        return 1;
    }
    
    int widget_status = flux_add_widget(win_id, "square", WIDGET_RECT);

    if (widget_status != 0) {
        printf("test.c: failed to create widget\n");

        flux_shutdown();

        return 1;
    }

    flux_set_widget_color(win_id, "square", "#ff0000ff");
    flux_set_widget_geometry(win_id, "square", 300, 300, 200, 200, 10, -1);

    flux_render_window(win_id);

    sleep(3);
    
    flux_remove_widget(win_id, "square");
    flux_destroy_window(win_id);
    flux_shutdown();
    
    return 0;
}