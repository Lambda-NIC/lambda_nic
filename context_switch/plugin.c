#include <stdlib.h>
#include <pif_plugin.h>
#include <pif_plugin_metadata.h>
#include <pif_common.h>
#include <memory.h>
#include <string.h>
#include <stdint.h>


int pif_plugin_serve_request1(EXTRACTED_HEADERS_T *headers,
                             MATCH_DATA_T *match_data)
{
    // Get the payload
    PIF_PLUGIN_pload_T *pload = pif_plugin_hdr_get_pload(headers);
    pload->__p_0 = 1751726624;
    return PIF_PLUGIN_RETURN_FORWARD;
}

int pif_plugin_serve_request2(EXTRACTED_HEADERS_T *headers,
                             MATCH_DATA_T *match_data)
{
    // Get the payload
    PIF_PLUGIN_pload_T *pload = pif_plugin_hdr_get_pload(headers);
    pload->__p_0 = 1652122938;
    return PIF_PLUGIN_RETURN_FORWARD;
}

int pif_plugin_serve_request3(EXTRACTED_HEADERS_T *headers,
                             MATCH_DATA_T *match_data)
{
    // Get the payload
    PIF_PLUGIN_pload_T *pload = pif_plugin_hdr_get_pload(headers);
    pload->__p_0 = 2003135587;
    pload->__p_1 = 1869440314;
    return PIF_PLUGIN_RETURN_FORWARD;
}


