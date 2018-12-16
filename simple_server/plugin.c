#include <stdint.h>
#include <nfp/me.h>
#include <nfp/mem_atomic.h>
#include <pif_common.h>

#include "pif_plugin.h"

/*
 * Simple server returning request.
 */

// we define a static return string
// Should this be shared?
static uint8_t reply_string[] = {'h', 'i', ':', ' '};

#define REPLY_LEN 4
#define UDP_HDR_LEN 8

int pif_plugin_serve_request(EXTRACTED_HEADERS_T *headers,
                             MATCH_DATA_T *match_data)
{
    //uint8_t tmp[16];
    //uint32_t p_len;

    // Get the payload
    PIF_PLUGIN_ipv4_T *ipv4 = pif_plugin_hdr_get_ipv4(headers);
    PIF_PLUGIN_udp_T *udp = pif_plugin_hdr_get_udp(headers);
    PIF_PLUGIN_pload_T *pload = pif_plugin_hdr_get_pload(headers);
    //uint8_t *ptr = (void *) pload;
    //p_len = udp->length_ - UDP_HDR_LEN;

    //pload->__p_1 = pload->__p_0;
    pload->__p_0 = 1751726624;
    
    // Copy the payload into temp mem.
    //memcpy_mem_mem(&tmp, ptr, p_len);
 
    // Copy the reply
    //memcpy_mem_mem(ptr, &reply_string, REPLY_LEN);
    //ptr += REPLY_LEN;

    // Copy back the payload
    //memcpy_mem_mem(ptr, &tmp, p_len);

    // Update the length on the header.
    //udp->length_ += REPLY_LEN;
    //ipv4->totalLen += REPLY_LEN;

    return PIF_PLUGIN_RETURN_FORWARD;
}

