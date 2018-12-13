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
static __lmem uint8_t search_string[] = {'hello: '};

#define REPLY_LEN 7

int pif_plugin_serve_request(EXTRACTED_HEADERS_T *headers,
                             MATCH_DATA_T *match_data)
{
    
    // Get the payload
    PIF_PLUGIN_payload_T *payload = pif_plugin_hdr_get_payload(headers);

    __lmem static uint8_t[50] payload;
    __xread uint32_t p_len;

    mem_read32(&p_len, &payload->p_len, sizeof(p_len));

    mem_read8(&payload, &payload->p, p_len);

    mem_write8(&payload->p, &search_string, REPLY_LEN);
    mem_write8((&payload->p) + REPLY_LEN, &payload, p_len);

    return PIF_PLUGIN_RETURN_FORWARD;
}
