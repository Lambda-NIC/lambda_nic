#include <pif_plugin.h>
#include <pif_plugin_metadata.h>

#include <stdint.h>
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
const char delims[] = "\r\n";

/*--------------------------------------------------------------------------------------*/
/*
 * Defines
 */

#define REPLY_LEN 4
#define UDP_HDR_LEN 8
#define MAC_CHAN_PER_PORT   8
#define TMQ_PER_PORT        (MAC_CHAN_PER_PORT * 8)
#define MAC_TO_PORT(x)      (x / MAC_CHAN_PER_PORT)
#define PORT_TO_TMQ(x)      (x * TMQ_PER_PORT)

#define TCP_DATA_LEN 10
#define UDP_DATA_LEN 10


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
__import __shared __cls struct ctm_pkt_credits ctm_credits;

/* counters for out of credit situations */
volatile __export __mem uint64_t gen_pkt_ctm_wait;
volatile __export __mem uint64_t gen_pkt_blm_wait;
volatile __export __mem uint32_t pif_mu_len = 0;

int first = 1;

/*------------------------------------------------------------------------*/
#define SWAP(ports) (((ports & 0xFFFF0000) >> 16) | ((ports & 0xFFFF) << 16))

struct packet_tx_eth_ip_tcp {
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

struct packet_tx_eth_ip_udp {
	union {
		__packed struct {
			uint16_t        eth_dst_hi;
			uint32_t        eth_dst_lo;
			uint32_t        eth_src_hi;
			uint16_t        eth_src_lo;
			uint16_t        eth_type;
			struct ip4_hdr  ip;
			struct udp_hdr  udp;
			uint8_t         udp_data[UDP_DATA_LEN];
		};
		uint32_t            __raw[16];
	};
};


/*
 * Packet metadata operations
 */
static void build_tx_meta(__lmem struct nbi_meta_catamaran *nbi_meta,
						  uint8_t ctm_buf_size)
{
	int pkt_num;
	__xread blm_buf_handle_t buf;
	int blq = pif_pkt_info_global.bls;

	reg_zero(nbi_meta->__raw, sizeof(struct nbi_meta_catamaran));

	/*
     * Poll for a CTM buffer until one is returned
     */
	while (1) {
		pkt_num = pkt_ctm_alloc(&ctm_credits, __ISLAND, ctm_buf_size, 1, 1);
		if (pkt_num != CTM_ALLOC_ERR)
			break;
		mem_incr64((__mem void *) gen_pkt_ctm_wait);		
		sleep(BACKOFF_SLEEP);
	}
	
	/*
     * Poll for MU buffer until one is returned.
     */
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
	/* packet metadata, always goes at start of ctm buffer */
	__lmem struct nbi_meta_catamaran mdata;
	/* this is the inline packet modifier data, ensures 8B alignment */
	__gpr struct pkt_ms_info msi;
	/* transfer registers for copying out packet data to ctm */
	__xwrite uint32_t xwr[32];
	/* point to packet data in CTM */
	__mem char *pbuf;
	/* we take care to start the packet on 8B alignment + 4B
     * as the egress prepend is 4B this amounts to an offset
     * of 8B which means the packet modification script is a nop
     */
	int pkt_offset = PKT_NBI_OFFSET + MAC_EGRESS_PREPEND_SIZE;
	int meta_len = sizeof(struct nbi_meta_catamaran);
	uint32_t nbi = 0;
	uint32_t out_port = 0; /* 0 or 1 for port 0, 1 */
			 
	/* Allocate packet and write out packet metadata to packet buffer */
	build_tx_meta(&mdata, PKT_CTM_SIZE_256);
	reg_cp((void*)xwr, (void*)&mdata, meta_len);
	pbuf = pkt_ctm_ptr40(mdata.pkt_info.isl, mdata.pkt_info.pnum, 0);
	mem_write32(xwr, pbuf, meta_len);			
		
	/* copy and write out the packet data into the packet buffer */
	reg_cp((void*)xwr, (void *)mbuf, len);
	mem_write32(xwr, pbuf + pkt_offset, len);

	/* set up the packet modifier to trim bytes for alignment */
	msi = pkt_msd_write(pbuf, PKT_NBI_OFFSET);
	
	/* send the packet */
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
	/* We write to packet data here and copy it into the ctm buffer */
	__lmem struct packet_tx_eth_ip_tcp Pdata;

	/* Build up the packet */
	reg_zero(Pdata.__raw, sizeof(struct packet_tx_eth_ip_tcp));
 
	Pdata.eth_dst_hi = 0x9090;
	Pdata.eth_dst_lo = 0x90909090;
	Pdata.eth_src_hi = 0x00150015;
	Pdata.eth_src_lo = 0x0015;
	Pdata.eth_type = NET_ETH_TYPE_IPV4;

	Pdata.ip.ver = 4;
	Pdata.ip.hl = 5;
	Pdata.ip.tos = 0;
	Pdata.ip.len = sizeof(Pdata.ip) +
		sizeof(Pdata.tcp) + sizeof(Pdata.tcp_data);
	Pdata.ip.frag = 0;
	Pdata.ip.ttl = 64;
	Pdata.ip.proto = 6;
	Pdata.ip.sum = 0;  /* Let MAC take care of this */
	Pdata.ip.src = srcAddr;
	Pdata.ip.dst = dstAddr;

	Pdata.tcp.sport = srcPort;
	Pdata.tcp.dport = dstPort;
	Pdata.tcp.seq = seqNum;
	Pdata.tcp.ack = ackNum;
	Pdata.tcp.off = 5;
	Pdata.tcp.flags = NET_TCP_FLAG_SYN | NET_TCP_FLAG_FIN;
	Pdata.tcp.win = 6000;
	Pdata.tcp.sum = 0; /* Let MAC take care of this too */
	Pdata.tcp.urp = 0;
 	
	/* fill up payload if required */
	for (i = 0; i < TCP_DATA_LEN; i++)
		Pdata.tcp_data[i] = i;
	
	
	send_packet(&(Pdata.__raw), sizeof(struct packet_tx_eth_ip_tcp));	
}


static void
send_udp_packet(uint32_t srcAddr, uint32_t dstAddr,
				uint16_t srcPort, uint16_t dstPort)
{
	int i;
	/* We write to packet data here and copy it into the ctm buffer */
	__lmem struct packet_tx_eth_ip_udp Pdata;

	/* Build up the packet */
	reg_zero(Pdata.__raw, sizeof(struct packet_tx_eth_ip_tcp));
 
	Pdata.eth_dst_hi = 0x9090;
	Pdata.eth_dst_lo = 0x90909090;
	Pdata.eth_src_hi = 0x00150015;
	Pdata.eth_src_lo = 0x0015;
	Pdata.eth_type = NET_ETH_TYPE_IPV4;

	Pdata.ip.ver = 4;
	Pdata.ip.hl = 5;
	Pdata.ip.tos = 0;
	Pdata.ip.len = sizeof(Pdata.ip) +
		sizeof(Pdata.udp) + sizeof(Pdata.udp_data);
	Pdata.ip.frag = 0;
	Pdata.ip.ttl = 64;
	Pdata.ip.proto = 6;
	Pdata.ip.sum = 0;  /* Let MAC take care of this */
	Pdata.ip.src = srcAddr;
	Pdata.ip.dst = dstAddr;

	Pdata.udp.sport = srcPort;
	Pdata.udp.dport = dstPort;
    Pdata.udp.sum = 0; // Ignore checksum
    Pdata.udp.len = sizeof(Pdata.udp) + sizeof(Pdata.udp_data);
 	
	/* fill up payload if required */
	for (i = 0; i < UDP_DATA_LEN; i++)
		Pdata.udp_data[i] = i;
	
	
	send_packet(&(Pdata.__raw), sizeof(struct packet_tx_eth_ip_udp));	
}

int pif_plugin_send_pkt(EXTRACTED_HEADERS_T *headers,
                        MATCH_DATA_T *match_data)
{
    // Get the payload
    PIF_PLUGIN_ipv4_T *ipv4 = pif_plugin_hdr_get_ipv4(headers);
    PIF_PLUGIN_udp_T *udp = pif_plugin_hdr_get_udp(headers);
    PIF_PLUGIN_pload_T *pload = pif_plugin_hdr_get_pload(headers);

    send_udp_packet(ipv4->dstAddr, ipv4->srcAddr, udp->dstPort, udp->srcPort);


    return PIF_PLUGIN_RETURN_FORWARD;
}


int pif_plugin_serve_request(EXTRACTED_HEADERS_T *headers,
                             MATCH_DATA_T *match_data)
{
    // Get the payload
    PIF_PLUGIN_ipv4_T *ipv4 = pif_plugin_hdr_get_ipv4(headers);
    PIF_PLUGIN_udp_T *udp = pif_plugin_hdr_get_udp(headers);
    PIF_PLUGIN_pload_T *pload = pif_plugin_hdr_get_pload(headers);
    //uint8_t *ptr = (void *) pload;
    //p_len = udp->length_ - UDP_HDR_LEN;

    //uint8_t tmp[16];
    //uint32_t p_len;

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


int pif_plugin_payload_scan(EXTRACTED_HEADERS_T *headers,
                            MATCH_DATA_T *match_data)
{
    __mem uint8_t *payload;
    __xread uint32_t pl_data[CHUNK_LW];
    __lmem uint32_t pl_mem[CHUNK_LW];
    int search_progress = 0;
    int i, count, to_read;
    uint32_t mu_len, ctm_len;

    /* figure out how much data is in external memory vs ctm */

    if (pif_pkt_info_global.split) { /* payload split to MU */
        uint32_t sop; /* start of packet offset */
        sop = PIF_PKT_SOP(pif_pkt_info_global.pkt_buf, pif_pkt_info_global.pkt_num);
        mu_len = pif_pkt_info_global.pkt_len - (256 << pif_pkt_info_global.ctm_size) + sop;
    } else /* no data in MU */
        mu_len = 0;

    /* debug info for mu_split */
    pif_mu_len = mu_len;

    /* get the ctm byte count:
     * packet length - offset to parsed headers - byte_count_in_mu
     * Note: the parsed headers are always in ctm
     */
    count = pif_pkt_info_global.pkt_len - pif_pkt_info_global.pkt_pl_off - mu_len;
    /* Get a pointer to the ctm portion */
    payload = pif_pkt_info_global.pkt_buf;
    /* point to just beyond the parsed headers */
    payload += pif_pkt_info_global.pkt_pl_off;

    while (count) {
        /* grab a maximum of chunk */
        to_read = count > CHUNK_B ? CHUNK_B : count;

        /* grab a chunk of memory into transfer registers */
        mem_read8(&pl_data, payload, to_read);

        /* copy from transfer registers into local memory
         * we can iterate over local memory, where transfer
         * registers we cant
         */
        for (i = 0; i < CHUNK_LW; i++)
            pl_mem[i] = pl_data[i];

        /* iterate over all the bytes and do the search 
        for (i = 0; i < to_read; i++) {
            uint8_t val = pl_mem[i/4] >> (8 * (3 - (i % 4)));

            if (val == search_string[search_progress])
                search_progress += 1;
            else
                search_progress = 0;

            if (search_progress >= sizeof(search_string)) {
                mem_incr32((__mem uint32_t *)&search_detections);
                mem_incr32((__mem uint32_t *)&search_ctm_detections);

                //
                //return PIF_PLUGIN_RETURN_DROP;
            }
        }
        */

        payload += to_read;
        count -= to_read;
    }

    /* same as above, but for mu. Code duplicated as a manual unroll */
    if (mu_len) {
        payload = (__addr40 void *)((uint64_t)pif_pkt_info_global.muptr << 11);
        /* Adjust payload size depending on the ctm size for the packet */
        payload += 256 << pif_pkt_info_global.ctm_size;        
        count = mu_len;
        while (count) {
            /* grab a maximum of chunk */
            to_read = count > CHUNK_B ? CHUNK_B : count;

            /* grab a chunk of memory into transfer registers */
            mem_read8(&pl_data, payload, to_read);

            /* copy from transfer registers into local memory
             * we can iterate over local memory, where transfer
             * registers we cant
             */
            for (i = 0; i < CHUNK_LW; i++)
                pl_mem[i] = pl_data[i];

            /* iterate over all the bytes and do the search
            for (i = 0; i < to_read; i++) {
                uint8_t val = pl_mem[i/4] >> (8 * (3 - (i % 4)));

                if (val == search_string[search_progress])
                    search_progress += 1;
                else
                    search_progress = 0;

                if (search_progress >= sizeof(search_string)) {
                    mem_incr32((__mem uint32_t *)&search_detections);
                    mem_incr32((__mem uint32_t *)&search_mu_detections);

                    //
                    //return PIF_PLUGIN_RETURN_DROP;
                }
            }
            */

            payload += to_read;
            count -= to_read;
        }
    }

    return PIF_PLUGIN_RETURN_FORWARD;
}

// Function to generate random number.
int pif_plugin_gen_rand(EXTRACTED_HEADERS_T* headers, ACTION_DATA_T* action_data)
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

