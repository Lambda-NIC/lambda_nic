#include <stdlib.h>
#include <pif_plugin.h>
#include <pif_plugin_metadata.h>
#include <pif_common.h>

/*
 * Defines
 */
#define IMAGE_DIM 256
#define CLONE_GET_PORT 0x3333
#define CLONE_SET_PORT 0x4444

// Location to read and write images for grayscale program.
volatile __export __mem uint8_t image_input_ready = 0;
volatile __export __mem uint8_t image_output_ready = 0;
volatile __export __mem uint32_t input_image[IMAGE_DIM][IMAGE_DIM]; //RGBA
volatile __export __mem uint8_t output_image[IMAGE_DIM][IMAGE_DIM];

int pif_plugin_send_cache_pkt(EXTRACTED_HEADERS_T *headers,
                              MATCH_DATA_T *match_data) 
{
    PIF_PLUGIN_memcached_T *memcached = pif_plugin_hdr_get_memcached(headers);
    PIF_PLUGIN_udp_T *udp = pif_plugin_hdr_get_udp(headers);
    PIF_PLUGIN_ipv4_T *ipv4 = pif_plugin_hdr_get_ipv4(headers);

    memcached->__data_0 = 0x00010000;
    memcached->__data_1 = 0x00010000;

    if (udp->dstPort == CLONE_GET_PORT) {
        // "Get "
        memcached->__data_2 = 0x67657420;
        // "hey\r"
        memcached->__data_3 = 0x6865790D;
        // "\n"
        memcached->__data_4 = 0x0A000000;
        udp->length_ = 25;
        ipv4->totalLen = 57;
    } else if (udp->dstPort == CLONE_SET_PORT) {
        // "Set "
        memcached->__data_2 = 0x73657420;
        // "hey "
        memcached->__data_3 = 0x68657920;
        // "0 0 "
        memcached->__data_4 = 0x30203020;
        // "4\r\nd"
        memcached->__data_5 = 0x340D0A64;
        // "ude\r"
        memcached->__data_6 = 0x7564650D;
        // "\n"
        memcached->__data_7 = 0x0A000000;
        udp->length_ = 37;
        ipv4->totalLen = 57;
    }
    return PIF_PLUGIN_RETURN_FORWARD;
}

int pif_plugin_serve_request(EXTRACTED_HEADERS_T *headers,
                             MATCH_DATA_T *match_data)
{
    int x, y;
    // Get the payload
    PIF_PLUGIN_pload_T *pload = pif_plugin_hdr_get_pload(headers);
    if (pload->jobId == 0) {
        pload->__p_0 = 1751726624;
    } else if (pload->jobId == 1) {
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
    }
    return PIF_PLUGIN_RETURN_FORWARD;
}