#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "osd.h"

int main(void)
{
    printf("====================================================\n");
    printf("     Native Wayland Volume OSD Library Demo\n");
    printf("====================================================\n\n");

    printf("1. Initializing OSD...\n");
    if (!osd_init()) {
        fprintf(stderr, "Failed to initialize OSD library.\n");
        return EXIT_FAILURE;
    }

    /* Configure maximum volume scale to 150% */
    osd_config_t config;
    osd_get_config(&config);
    config.position = OSD_POS_BOTTOM_CENTER;
    config.margin_y = 100;
    config.max_volume = 150; /* Configurable maximum volume scale up to 150% or higher */
    osd_set_config(&config);

    printf("2. Displaying volume at 25%%...\n");
    osd_show_volume(25, false);
    osd_delay_ms(1500);

    printf("3. Increasing volume to 85%%...\n");
    osd_show_volume(85, false);
    osd_delay_ms(1500);

    printf("4. Increasing volume to 135%% (Overamplification up to 150%%)...\n");
    osd_show_volume(135, false);
    osd_delay_ms(1500);

    printf("5. Muting volume...\n");
    osd_show_volume(135, true);

    printf("6. Waiting for auto-hide animation (3 seconds)...\n");
    osd_delay_ms(3000);

    printf("7. Cleaning up OSD resources...\n");
    osd_destroy();

    printf("\nDemo completed successfully!\n");
    return EXIT_SUCCESS;
}
