#define MAC_CHAN_PER_PORT   8
#define TMQ_PER_PORT        (MAC_CHAN_PER_PORT * 8)
#define MAC_TO_PORT(x)      (x / MAC_CHAN_PER_PORT)
#define PORT_TO_TMQ(x)      (x * TMQ_PER_PORT)
#define MAC_EGRESS_PREPEND_SIZE 0

/* credits for CTM */
__import  __cls struct ctm_pkt_credits ctm_credits;


/* counters for out of credit situations */
volatile __export __mem uint64_t gen_pkt_ctm_wait;
volatile __export __mem uint64_t gen_pkt_blm_wait;



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

    Pdata->eth_dst_hi  = 0x1515;
    Pdata->eth_dst_lo  = 0x15151515;
    Pdata->eth_src_hi  = 0x14141414;
    Pdata->eth_src_lo  = 0x1414;
    Pdata->eth_type = NET_ETH_TYPE_IPV4;

    Pdata->ip.ver = 4;
    Pdata->ip.hl = 5;
    Pdata->ip.tos = 0;
    Pdata->ip.len = sizeof(Pdata->ip) +
                    sizeof(Pdata->tcp) +
                    sizeof(Pdata->tcp_data);
    Pdata->ip.frag = 0;
    Pdata->ip.ttl = 64;
    Pdata->ip.proto = 6;
    Pdata->ip.sum = 0;  // Let MAC take care of this
    Pdata->ip.src = 0x11111111;
    Pdata->ip.dst = 0x22222222;

    Pdata->tcp.sport = 1111;
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
        sleep(1000);
        mem_incr64((__mem void *) gen_pkt_ctm_wait);
    }
    // Poll for MU buffer
    while (blm_buf_alloc(&buf, blq) != 0) {
        sleep(1000);
        mem_incr64((__mem void *) gen_pkt_blm_wait);
    }

    nbi_meta->pkt_info.isl = __ISLAND;
    nbi_meta->pkt_info.pnum = pkt_num;
    nbi_meta->pkt_info.bls = blq;
    nbi_meta->pkt_info.muptr = buf;
}


int pif_plugin_send_pkt(EXTRACTED_HEADERS_T *headers,
                        MATCH_DATA_T *match_data)
{
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

    int pkt_offset = PKT_NBI_OFFSET + MAC_EGRESS_PREPEND_SIZE;
    int pkt_len = sizeof(struct packet_tx_eth_ip);
    int meta_len = sizeof(struct nbi_meta_catamaran);
    int i, j;
    uint32_t nbi = 0;
    uint32_t out_port = 0; // 0 or 1 for port 0, 1

	for (i = 1; i <= 10; i++) {
        // Allocate packet and write out packet metadata to packet buffer
		build_tx_meta(&mdata);
		reg_cp((void*)xwr, (void*)&mdata, meta_len);
		pbuf = pkt_ctm_ptr40(mdata.pkt_info.isl, mdata.pkt_info.pnum, 0);
		mem_write32(xwr, pbuf, meta_len);

        // Build up the packet
		build_tx_ip(&pdata);

		// Add some tcp payload data
		for (j = 0; j < TCP_DATA_LEN; j++)
			pdata.tcp_data[j] = j;

        // Copy and write out the packet data into the packet buffer
		reg_cp((void*)xwr, (void*)pdata.__raw, pkt_len);
		mem_write32(xwr, pbuf + pkt_offset, pkt_len);

        // Tell the mac to patch in the L3 and L4 checksums
		pkt_mac_egress_cmd_write(pbuf,
                                 PKT_NBI_OFFSET + MAC_EGRESS_PREPEND_SIZE,
                                 1, 1);

        // Set up the packet modifier to trim bytes for alignment
		msi = pkt_msd_write(pbuf, PKT_NBI_OFFSET);
        
        // Send the packet
		pkt_nbi_send(mdata.pkt_info.isl, mdata.pkt_info.pnum, &msi,
					 pkt_len + MAC_EGRESS_PREPEND_SIZE,
					 nbi, PORT_TO_TMQ(out_port),
					 mdata.seqr, mdata.seq, PKT_CTM_SIZE_256);

	}
    return PIF_PLUGIN_RETURN_DROP;
}
