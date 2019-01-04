#include <stdlib.h>
#include <pif_plugin.h>
#include <pif_plugin_metadata.h>
#include <pif_common.h>
#include <memory.h>
#include <string.h>
#include <stdint.h>

/*
 * Defines
 */
#define IMAGE_DIM 256

// Location to read and write images for grayscale program.
volatile __export __emem uint8_t image_input_ready = 0;
volatile __export __emem uint8_t image_output_ready = 0;
volatile __export __emem uint32_t input_image[IMAGE_DIM][IMAGE_DIM]; // RGBA
volatile __export __emem uint8_t output_image[IMAGE_DIM][IMAGE_DIM];

int pif_plugin_grayscale_img(EXTRACTED_HEADERS_T *headers,
                             MATCH_DATA_T *match_data)
{
    int x, y;
    while (!image_input_ready) {
        sleep(1000);
    }
    
    // Compute grayscale as average
    for (x = 0; x < IMAGE_DIM; x++) {
        for (y = 0; y < IMAGE_DIM; y++) {
            output_image[x][y] =
                ((uint8_t)(input_image[x][y] & 0xFF)/3 +
                 (uint8_t)((input_image[x][y] >> 8) & 0xFF)/3 +
                 (uint8_t)((input_image[x][y] >> 16) & 0xFF)/3);
        }
    }
    image_output_ready = 1;

    return PIF_PLUGIN_RETURN_FORWARD;
}
