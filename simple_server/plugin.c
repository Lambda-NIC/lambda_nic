#include <memory.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <pif_plugin.h>
#include <pif_plugin_metadata.h>
#include <nfp/me.h>
#include <nfp/cls.h>
#include <nfp/mem_atomic.h>
#include <pif_common.h>
#include <pkt_ops.h>

#include <blm.h>
#include <std/reg_utils.h>
#include <net/eth.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/udp.h>


/*
 * Simple server returning request.
 */

// we define a static return string
// Should this be shared?
//static uint8_t reply_string[] = {'h', 'i', ':', '\n'};

const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";

// set <key> <flags> <exptime> <bytes>\r\n<datablock>\r\n
const char setcmd[] = "set ";
const char setopts[] = "0 0 ";
// get <key>*\r\n
const char getcmd[] = "get ";
//const char delims[] = "\r\n";

/*--------------------------------------------------------------------------------------*/
/*
 * Defines
 */
#define IMAGE_DIM 256
#define IMAGE_COLORS 3
#define REPLY_LEN 4
#define UDP_HDR_LEN 8
#define ETH_BYTES 6

//#define MAC_CHAN_PER_PORT   8
#define MAC_CHAN_PER_PORT   4
#define TMQ_PER_PORT        (MAC_CHAN_PER_PORT * 8)
#define MAC_TO_PORT(x)      (x / MAC_CHAN_PER_PORT)
#define PORT_TO_TMQ(x)      (x * TMQ_PER_PORT)

#define TCP_DATA_LEN 10
#define UDP_DATA_LEN 86
#define TCP_PROTO    6
#define UDP_PROTO    17
#define DEFAULT_TTL  64
#define REG_CHUNK    32

#ifndef NBI
#define NBI 0
#endif



/* Payload chunk size in LW (32-bit) and bytes */
#define CHUNK_LW 8
#define CHUNK_B (CHUNK_LW * 4)


/* CTM credit defines */
#define CTM_ALLOC_ERR 0xffffffff

/* 4B mac egress prepend for L3/4 checksums are not used for now */
#define MAC_EGRESS_PREPEND_SIZE 0

/*
 * Globals
 */

/* credits for CTM */
//__import __shared __cls struct ctm_pkt_credits ctm_credits;

__import  __cls struct ctm_pkt_credits ctm_credits;


/* counters for out of credit situations */
volatile __export __mem uint64_t gen_pkt_ctm_wait;
volatile __export __mem uint64_t gen_pkt_blm_wait;

// MU Len for searcher
volatile __export __mem uint32_t pif_mu_len = 0;


// Location to read and write images for grayscale program.
volatile __export __emem uint8_t image_input_ready = 0;
volatile __export __emem uint8_t image_output_ready = 0;
volatile __export __emem uint32_t input_image[IMAGE_DIM][IMAGE_DIM]; // RGBA
volatile __export __emem uint8_t output_image[IMAGE_DIM][IMAGE_DIM];

int first = 1;

/*------------------------------------------------------------------------*/
#define SWAP(ports) (((ports & 0xFFFF0000) >> 16) | ((ports & 0xFFFF) << 16))

struct packet_tx_eth_ip_tcp {
	union {
		__packed struct {
            // Check if this matches what is defined in pif_headers.h
			uint8_t         eth_dst[ETH_BYTES];
			uint8_t         eth_src[ETH_BYTES];
			uint16_t        eth_type;
			struct ip4_hdr  ip;
			struct tcp_hdr  tcp;
			uint8_t         tcp_data[TCP_DATA_LEN];
		};
		uint32_t            __raw[32];
	};
};

struct packet_tx_eth_ip_udp {
	union {
		__packed struct {
            // Check if this matches what is defined in pif_headers.h
			//uint8_t         eth_dst[ETH_BYTES];
			//uint8_t         eth_src[ETH_BYTES];
            uint16_t        eth_dst_hi;
            uint32_t        eth_dst_low;
            uint16_t        eth_src_hi;
            uint32_t        eth_src_low;
			uint16_t        eth_type;
			struct ip4_hdr  ip;
			struct udp_hdr  udp;
			uint8_t         udp_data[UDP_DATA_LEN];
		};
		uint32_t            __raw[32];
	};
};


/*
 * Packet metadata operations
static void build_tx_meta(__lmem struct nbi_meta_catamaran *nbi_meta,
						  uint8_t ctm_buf_size)
{
	int pkt_num;
	__xread blm_buf_handle_t buf;
	//int blq = pif_pkt_info_global.bls;
    int blq = 0;

	reg_zero(nbi_meta->__raw, sizeof(struct nbi_meta_catamaran));

    // Poll for a CTM buffer until one is returned
	while (1) {
		pkt_num = pkt_ctm_alloc(&ctm_credits, __ISLAND, ctm_buf_size, 1, 1);
		if (pkt_num != CTM_ALLOC_ERR)
			break;
		mem_incr64((__mem void *) gen_pkt_ctm_wait);		
		sleep(BACKOFF_SLEEP);
	}

    //Poll for MU buffer until one is returned.
	while (blm_buf_alloc(&buf, blq) != 0) {
		mem_incr64((__mem void *) gen_pkt_blm_wait);
		sleep(BACKOFF_SLEEP);
	}	

	nbi_meta->pkt_info.isl = __ISLAND;
	nbi_meta->pkt_info.pnum = pkt_num;
	nbi_meta->pkt_info.bls = blq;
	nbi_meta->pkt_info.muptr = buf;
}

void
send_packet(__addr32 void* mbuf, int len)
{
	// packet metadata, always goes at start of ctm buffer
	__lmem struct nbi_meta_catamaran mdata;

	// this is the inline packet modifier data, ensures 8B alignment
	__gpr struct pkt_ms_info msi;

	// transfer registers for copying out packet data to ctm
	__xwrite uint32_t xwr[REG_CHUNK];

	// point to packet data in CTM
	__mem char *pbuf;

    int i;
    int left_over;
    int curr_offset;

	//we take care to start the packet on 8B alignment + 4B
    //as the egress prepend is 4B this amounts to an offset
    //of 8B which means the packet modification script is a nop
	int pkt_offset = PKT_NBI_OFFSET + MAC_EGRESS_PREPEND_SIZE;
	int meta_len = sizeof(struct nbi_meta_catamaran);
	uint32_t nbi = 0;

    // Use port 1, since it is connected.
	uint32_t out_port = 1;
			 
	// Allocate packet and write out packet metadata to packet buffer
	build_tx_meta(&mdata, PKT_CTM_SIZE_256);
	reg_cp((void*)xwr, (void*)&mdata, meta_len);
	pbuf = pkt_ctm_ptr40(mdata.pkt_info.isl, mdata.pkt_info.pnum, 0);
	mem_write32(xwr, pbuf, meta_len);			
		
	// Copy and write out the packet data into the packet buffer
    left_over = len;
    for (i = 0; i < len/REG_CHUNK; i++) {
        curr_offset = i * REG_CHUNK;
        if (left_over > REG_CHUNK) {
    	    reg_cp((void*)xwr, (void *)((char *)mbuf + curr_offset), REG_CHUNK);
	        mem_write32(xwr, pbuf + curr_offset + pkt_offset, REG_CHUNK);
            left_over -= REG_CHUNK;
        } else {
    	    reg_cp((void*)xwr, (void *)((char *)mbuf + curr_offset), REG_CHUNK);
	        mem_write8(xwr, pbuf + curr_offset + pkt_offset, left_over);
        }
    }

    //reg_cp((void*)xwr, (void*)pdata.__raw, pkt_len);
    //mem_write32(xwr, pbuf + pkt_offset, pkt_len);

	// set up the packet modifier to trim bytes for alignment
	msi = pkt_msd_write(pbuf, PKT_NBI_OFFSET);
	
	// send the packet
	pkt_nbi_send(mdata.pkt_info.isl, mdata.pkt_info.pnum, &msi,
				 len + MAC_EGRESS_PREPEND_SIZE,
				 nbi, PORT_TO_TMQ(out_port),
				 mdata.seqr, mdata.seq, PKT_CTM_SIZE_256);
}


static void
send_tcp_packet(uint32_t srcAddr, uint32_t dstAddr,
				uint16_t srcPort, uint16_t dstPort,
				uint32_t seqNum,  uint32_t ackNum)
{
	int i;
	// We write to packet data here and copy it into the ctm buffer
	__lmem struct packet_tx_eth_ip_tcp Pdata;

	// Build up the packet
	reg_zero(Pdata.__raw, sizeof(struct packet_tx_eth_ip_tcp));
 
    for (i = 0; i < ETH_BYTES; i++) {
	    Pdata.eth_dst[i] = 0x90;
    }
    for (i = 0; i < ETH_BYTES/2; i += 2) {
        Pdata.eth_src[i] = 0x00;
        Pdata.eth_src[i + 1] = 0x15;
    }
	Pdata.eth_type = NET_ETH_TYPE_IPV4;

	Pdata.ip.ver = 4;
	Pdata.ip.hl = 5;
	Pdata.ip.tos = 0;
	Pdata.ip.len = sizeof(Pdata.ip) +
		sizeof(Pdata.tcp) + sizeof(Pdata.tcp_data);
	Pdata.ip.frag = 0;
	Pdata.ip.ttl = DEFAULT_TTL;
	Pdata.ip.proto = TCP_PROTO;
	Pdata.ip.sum = 0;  // Let MAC take care of this
	Pdata.ip.src = srcAddr;
	Pdata.ip.dst = dstAddr;

	Pdata.tcp.sport = srcPort;
	Pdata.tcp.dport = dstPort;
	Pdata.tcp.seq = seqNum;
	Pdata.tcp.ack = ackNum;
	Pdata.tcp.off = 5;
	Pdata.tcp.flags = NET_TCP_FLAG_SYN | NET_TCP_FLAG_FIN;
	Pdata.tcp.win = 6000;
	Pdata.tcp.sum = 0; // Let MAC take care of this too
	Pdata.tcp.urp = 0;
 	
	// fill up payload if required 
	for (i = 0; i < TCP_DATA_LEN; i++)
		Pdata.tcp_data[i] = i;
	
	
	send_packet(&(Pdata.__raw), sizeof(struct packet_tx_eth_ip_tcp));	
}

// Sets the udp packet to be a get packet
static void create_get_req(struct packet_tx_eth_ip_udp *Pdata,
                           char* key, int key_len) {
    int i;
    int pos = 0;
    for (i = 0; i < sizeof(getcmd); i++) {
        Pdata->udp_data[pos] = getcmd[i];
        pos++;
    }
    for (i = 0; i < key_len; i++) {
        Pdata->udp_data[pos] = key[i];
        pos++;
    }
    Pdata->udp_data[pos] = 0x0D;
    pos++;
    Pdata->udp_data[pos] = 0x0A;
    pos++;
    //
    //for (i = 0; i < sizeof(delims); i++) {
    //    Pdata->udp_data[pos] = delims[i];
    //    pos++;
    //}

    // Set the length correctly.
	Pdata->ip.len = sizeof(Pdata->ip) + sizeof(Pdata->udp) + pos;
    Pdata->udp.len = sizeof(Pdata->udp) + pos;
}

// Sets the udp packet to be a get packet
static void create_set_req(__lmem struct packet_tx_eth_ip_udp *Pdata,
                           char* key, int key_len, char* key_len_str,
                           char* value, int val_len, char* val_len_str)
{
    // set <key> <flags> <exptime> <bytes>\r\n<datablock>\r\n
    int i;
    int pos = 0;

    for (i = 0; i < sizeof(setcmd); i++) {
        Pdata->udp_data[pos] = setcmd[i];
        pos++;
    }
    for (i = 0; i < key_len; i++) {
        Pdata->udp_data[pos] = key[i];
        pos++;
    }
    // Adding a space after the key.
    // Done when making the key.
    //Pdata->udp_data[pos] = " ";
    //pos++;
    for (i = 0; i < sizeof(setopts); i++) {
        Pdata->udp_data[pos] = setopts[i];
        pos++;
    }
    for (i = 0; i < strlen_mem(val_len_str); i++) {
        Pdata->udp_data[pos] = val_len_str[i];
        pos++;
    }
    Pdata->udp_data[pos] = 0x0D;
    pos++;
    Pdata->udp_data[pos] = 0x0A;
    pos++;
    
    //for (i = 0; i < sizeof(delims); i++) {
    //    Pdata->udp_data[pos] = delims[i];
    //    pos++;
    //}
    
    for (i = 0; i < val_len; i++) {
        Pdata->udp_data[pos] = value[i];
        pos++;
    }
    Pdata->udp_data[pos] = 0x0D;
    pos++;
    Pdata->udp_data[pos] = 0x0A;
    pos++;
    
    //for (i = 0; i < sizeof(delims); i++) {
    //    Pdata->udp_data[pos] = delims[i];
    //    pos++;
    //}
    
    // Set the length correctly.
	Pdata->ip.len = sizeof(Pdata->ip) + sizeof(Pdata->udp) + pos;
    Pdata->udp.len = sizeof(Pdata->udp) + pos;
}


static void
create_udp_packet(
    __lmem struct packet_tx_eth_ip_udp *Pdata,
    //uint8_t eth_dst[ETH_BYTES],
    //uint8_t eth_src[ETH_BYTES],
    uint32_t src_addr, uint32_t dst_addr,
	uint16_t src_port, uint16_t dst_port)
{
	// Build up the packet 
    int left_over = sizeof(struct packet_tx_eth_ip_udp); //128 byte packet
    int curr_offset;
    int len = left_over;
    int i;
    // Zero out the memory locations in the LMEM.
    // reg_zero works for LMEM as well.
    for (i = 0; i < len/REG_CHUNK; i++) {
        curr_offset = i * REG_CHUNK;
        if (left_over > REG_CHUNK) {
            reg_zero(Pdata->__raw + curr_offset, REG_CHUNK);
            left_over -= REG_CHUNK;
        } else {
            reg_zero(Pdata->__raw + curr_offset, REG_CHUNK);
        }
    }
	//reg_zero(Pdata->__raw, sizeof(struct packet_tx_eth_ip_udp));
    
    //memcpy_lmem_mem(&(Pdata->eth_dst), &eth_dst, ETH_BYTES);
    //memcpy_lmem_mem(&(Pdata->eth_src), &eth_src, ETH_BYTES);
    Pdata->eth_dst_hi  = 0xB026;
    Pdata->eth_dst_low = 0x281A7560;
    Pdata->eth_src_hi  = 0x0015;
    Pdata->eth_src_low = 0x4D001101;
	Pdata->eth_type = NET_ETH_TYPE_IPV4;

	Pdata->ip.ver = 4;
	Pdata->ip.hl = 5;
	Pdata->ip.tos = 0;

    // This is set when payload is set
	//Pdata.ip.len = sizeof(Pdata.ip) + sizeof(Pdata.udp).
	Pdata->ip.frag = 0;
	Pdata->ip.ttl = DEFAULT_TTL;
	Pdata->ip.proto = UDP_PROTO;
	Pdata->ip.sum = 0;  // Let MAC take care of this
	Pdata->ip.src = src_addr;
	Pdata->ip.dst = dst_addr;

	Pdata->udp.sport = src_port;
	Pdata->udp.dport = dst_port;
    Pdata->udp.sum = 0; // Ignore checksum
    
    // Also set when payload is set.
    //Pdata.udp.len = sizeof(Pdata.udp) + sizeof(Pdata.udp_data);
	
}
*/


struct packet_tx_eth_ip {
    union {
        __packed struct {
            uint16_t        eth_dst_hi;
            uint32_t        eth_dst_lo;
            uint32_t        eth_src_hi;
            uint16_t        eth_src_lo;
            uint16_t        eth_type;
            struct ip4_hdr  ip;
            struct tcp_hdr  tcp;
            uint8_t         tcp_data[TCP_DATA_LEN];
        };
        uint32_t            __raw[16];
    };
};


static void build_tx_ip(__lmem struct packet_tx_eth_ip *Pdata)
{
    reg_zero(Pdata->__raw, sizeof(struct packet_tx_eth_ip));

    Pdata->eth_dst_hi  = 0xB026;
    Pdata->eth_dst_lo  = 0x281A7560;
    Pdata->eth_src_hi  = 0x00154D00;
    Pdata->eth_src_lo  = 0x1101;

    //Pdata->eth_dst_hi  = 0x0015;
    //Pdata->eth_dst_lo  = 0x4D001101;
    //Pdata->eth_src_hi  = 0xB026281A;
    //Pdata->eth_src_lo  = 0x7560;

    Pdata->eth_type = NET_ETH_TYPE_IPV4;

    Pdata->ip.ver = 4;
    Pdata->ip.hl = 5;
    Pdata->ip.tos = 0;
    Pdata->ip.len = sizeof(Pdata->ip) +
                    sizeof(Pdata->tcp) +
                    sizeof(Pdata->tcp_data);
    Pdata->ip.frag = 0;
    Pdata->ip.ttl = DEFAULT_TTL;
    Pdata->ip.proto = TCP_PROTO;
    Pdata->ip.sum = 0;  // Let MAC take care of this
    Pdata->ip.src = 0x14141565;
    Pdata->ip.dst = 0x1E1E1E69;

    //Pdata->ip.src = 0x1E1E1E69;
    //Pdata->ip.dst = 0x14141565;

    Pdata->tcp.sport = 2222;
    Pdata->tcp.dport = 2222;
    Pdata->tcp.seq = 0x11223344;
    Pdata->tcp.ack = 0x55667788;
    Pdata->tcp.off = 5;
    Pdata->tcp.flags = NET_TCP_FLAG_ACK;
    Pdata->tcp.win = 6000;
    Pdata->tcp.sum = 0; // Let MAC take care of this too
    Pdata->tcp.urp = 0;
}

/*
 * Packet metadata operations
 */
static void build_tx_meta(__lmem struct nbi_meta_catamaran *nbi_meta)
{
    __xread blm_buf_handle_t buf;
    int pkt_num;
    int blq = 0;

    reg_zero(nbi_meta->__raw, sizeof(struct nbi_meta_catamaran));

    // Poll for CTM Buffer
    while (1) {
        pkt_num = pkt_ctm_alloc(&ctm_credits, __ISLAND, PKT_CTM_SIZE_256, 1, 1);
        if (pkt_num != CTM_ALLOC_ERR)
            break;
        sleep(BACKOFF_SLEEP);
        mem_incr64((__mem void *) gen_pkt_ctm_wait);
    }
    // Poll for MU buffer
    while (blm_buf_alloc(&buf, blq) != 0) {
        sleep(BACKOFF_SLEEP);
        mem_incr64((__mem void *) gen_pkt_blm_wait);
    }

    nbi_meta->pkt_info.isl = __ISLAND;
    nbi_meta->pkt_info.pnum = pkt_num;
    nbi_meta->pkt_info.bls = blq;
    nbi_meta->pkt_info.muptr = buf;
}




int pif_plugin_send_cache_set_pkt(EXTRACTED_HEADERS_T *headers,
                                  MATCH_DATA_T *match_data)
{

    PIF_PLUGIN_ipv4_T *ipv4 = pif_plugin_hdr_get_ipv4(headers);
    //PIF_PLUGIN_udp_T *udp = pif_plugin_hdr_get_udp(headers);
    //PIF_PLUGIN_pload_T *pload = pif_plugin_hdr_get_pload(headers);

    // packet metadata, always goes at start of ctm buffer
    __lmem struct nbi_meta_catamaran mdata;
    // We write to packet data here and copy it into the ctm buffer
    __lmem struct packet_tx_eth_ip pdata;
    // this is the inline packet modifier data, ensures 8B alignment
    __gpr struct pkt_ms_info msi;
    // transfer registers for copying out packet data to ctm
    __xwrite uint32_t xwr[32];
    // point to packet data in CTM
    __mem char *pbuf;


    int i, j;

    int pkt_len = sizeof(struct packet_tx_eth_ip);

    int meta_len = sizeof(struct nbi_meta_catamaran);

    //uint32_t out_port = 0; /* 0 or 1 for port 0, 1 */
    __gpr int out_port = 0;

    /* tell the mac to patch in the L3 and L4 checksums */
    __gpr int pkt_off = PKT_NBI_OFFSET; // + MAC_PREPEND_BYTES;

	for (i = 1; i <= 60; i++) {

        /* Allocate packet and write out packet metadata to packet buffer */
		build_tx_meta(&mdata);
		reg_cp((void*)xwr, (void*)&mdata, meta_len);
		pbuf = pkt_ctm_ptr40(mdata.pkt_info.isl, mdata.pkt_info.pnum, 0);
		mem_write32(xwr, pbuf, meta_len);

        /* Build up the packet */
		build_tx_ip(&pdata);

		/* Add some tcp payload data */
		for (j = 0; j < TCP_DATA_LEN; j++)
			pdata.tcp_data[j] = j;

        /* copy and write out the packet data into the packet buffer */
		reg_cp((void*)xwr, (void*)pdata.__raw, pkt_len);
		mem_write32(xwr, pbuf + pkt_off, pkt_len);


		pkt_mac_egress_cmd_write(pbuf, pkt_off, 1, 1);

        /* set up the packet modifier to trim bytes for alignment */
		msi = pkt_msd_write(pbuf, pkt_off);

        /* send the packet */
		pkt_nbi_send(mdata.pkt_info.isl,
                     mdata.pkt_info.pnum,
                     &msi,
					 pkt_len + MAC_EGRESS_PREPEND_SIZE,
					 NBI,
                     PORT_TO_TMQ(out_port),
					 mdata.seqr,
                     mdata.seq,
                     PKT_CTM_SIZE_256);

	}
    ipv4->ttl = i;
    return PIF_PLUGIN_RETURN_FORWARD;
}

int pif_plugin_grayscale_img(EXTRACTED_HEADERS_T *headers,
                             MATCH_DATA_T *match_data)
{
    int x, y, c;
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


/*
int pif_plugin_send_cache_set_pkt(EXTRACTED_HEADERS_T *headers,
                                  MATCH_DATA_T *match_data)
{
    // Get the payload
    PIF_PLUGIN_eth_T *eth = pif_plugin_hdr_get_eth(headers);
    PIF_PLUGIN_ipv4_T *ipv4 = pif_plugin_hdr_get_ipv4(headers);
    PIF_PLUGIN_udp_T *udp = pif_plugin_hdr_get_udp(headers);
    PIF_PLUGIN_pload_T *pload = pif_plugin_hdr_get_pload(headers);

    char key[] = "hello ";
    char val[] = "beautiful world";

    char key_len[] = "5";
    char val_len[] = "15";
	// We write to packet data here and copy it into the ctm buffer
	__lmem struct packet_tx_eth_ip_udp Pdata;
    // Revert eth, srcAddr and Port
    create_udp_packet(
        &Pdata,
        //(uint8_t *) eth + ETH_BYTES, ((uint8_t *)eth),
        ipv4->dstAddr, ipv4->srcAddr,
        udp->dstPort, udp->srcPort
    );

	create_set_req(&Pdata,
        (char *)&key, 5, (char *)&key_len,
        (char *)&val, 15, (char *)&val_len
    );
	send_packet(&(Pdata.__raw), sizeof(struct packet_tx_eth_ip_udp));
    return PIF_PLUGIN_RETURN_DROP;
}
*/

int pif_plugin_serve_request(EXTRACTED_HEADERS_T *headers,
                             MATCH_DATA_T *match_data)
{
    // Get the payload
    //PIF_PLUGIN_ipv4_T *ipv4 = pif_plugin_hdr_get_ipv4(headers);
    //PIF_PLUGIN_udp_T *udp = pif_plugin_hdr_get_udp(headers);
    PIF_PLUGIN_pload_T *pload = pif_plugin_hdr_get_pload(headers);
    pload->__p_0 = 1751726624;
    return PIF_PLUGIN_RETURN_FORWARD;
}

/*
static int payload_scan(EXTRACTED_HEADERS_T *headers,
                        MATCH_DATA_T *match_data)
{
    __mem uint8_t *payload;
    __xread uint32_t pl_data[CHUNK_LW];
    __lmem uint32_t pl_mem[CHUNK_LW];
    int search_progress = 0;
    int i, count, to_read;
    uint32_t mu_len, ctm_len;

    //figure out how much data is in external memory vs ctm

    if (pif_pkt_info_global.split) { //payload split to MU
        uint32_t sop; // start of packet offset
        sop = PIF_PKT_SOP(pif_pkt_info_global.pkt_buf, pif_pkt_info_global.pkt_num);
        mu_len = pif_pkt_info_global.pkt_len - (256 << pif_pkt_info_global.ctm_size) + sop;
    } else // no data in MU
        mu_len = 0;

    // debug info for mu_split
    pif_mu_len = mu_len;

    // get the ctm byte count:
    // packet length - offset to parsed headers - byte_count_in_mu
    // Note: the parsed headers are always in ctm
    count = pif_pkt_info_global.pkt_len - pif_pkt_info_global.pkt_pl_off - mu_len;
    // Get a pointer to the ctm portion
    payload = pif_pkt_info_global.pkt_buf;
    //point to just beyond the parsed headers
    payload += pif_pkt_info_global.pkt_pl_off;

    while (count) {
        //grab a maximum of chunk
        to_read = count > CHUNK_B ? CHUNK_B : count;

        // grab a chunk of memory into transfer registers
        mem_read8(&pl_data, payload, to_read);

        //copy from transfer registers into local memory
        //we can iterate over local memory, where transfer
        //registers we cant
        for (i = 0; i < CHUNK_LW; i++)
            pl_mem[i] = pl_data[i];

        payload += to_read;
        count -= to_read;
    }

    //same as above, but for mu. Code duplicated as a manual unroll
    if (mu_len) {
        payload = (__addr40 void *)((uint64_t)pif_pkt_info_global.muptr << 11);
        //Adjust payload size depending on the ctm size for the packet
        payload += 256 << pif_pkt_info_global.ctm_size;        
        count = mu_len;
        while (count) {
            // grab a maximum of chunk
            to_read = count > CHUNK_B ? CHUNK_B : count;

            // grab a chunk of memory into transfer registers 
            mem_read8(&pl_data, payload, to_read);

            //copy from transfer registers into local memory
            //we can iterate over local memory, where transfer
            //registers we cant
            for (i = 0; i < CHUNK_LW; i++)
                pl_mem[i] = pl_data[i];

            payload += to_read;
            count -= to_read;
        }
    }

    return PIF_PLUGIN_RETURN_FORWARD;
}
*/

// Function to generate random number.
/*
static int gen_rand(EXTRACTED_HEADERS_T* headers, ACTION_DATA_T* action_data)
{
    uint32_t randval;

    if (first){
        first = 0;
        local_csr_write(local_csr_pseudo_random_number,(local_csr_read(local_csr_timestamp_low) & 0xffff) +1 );
        local_csr_read(local_csr_pseudo_random_number);
    }
    randval = local_csr_read(local_csr_pseudo_random_number);


    return PIF_PLUGIN_RETURN_FORWARD;
}
*/

