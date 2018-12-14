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
static __lmem uint8_t reply_string[] = {'h', 'i', ':', ' '};

#define REPLY_LEN 4

int pif_plugin_serve_request(EXTRACTED_HEADERS_T *headers,
                             MATCH_DATA_T *match_data)
{
    __lmem uint32_t tmp[16];
    __gpr uint32_t p_len;

    // Get the payload
    PIF_PLUGIN_pload_T *pload = pif_plugin_hdr_get_pload(headers);
    uint8_t *ptr = (void *) pload;

    p_len = PIF_HEADER_GET_pload___pLen(pload);

    // Copy the payload into temp mem.
    memcpy_lmem_mem(&tmp, ptr, p_len);

    // Copy the reply and the payload back on
    memcpy_mem_lmem(ptr, &reply_string, REPLY_LEN);
    ptr += REPLY_LEN;
    memcpy_mem_lmem(ptr, &tmp, p_len);

    return PIF_PLUGIN_RETURN_FORWARD;
}
