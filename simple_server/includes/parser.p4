
/*
  +-------------------------------------------
    Define parser
  +-------------------------------------------
*/


#define IPV4_TYPE 0x0800
#define UDP_PROTOCOL 0x11
#define TCP_PROTOCOL 0x06

#define SERVER_PORT 0x1111
#define INTERIM_PORT 0x2222
#define CLONE_GET_PORT 0x3333
#define CLONE_SET_PORT 0x4444
#define MEMCACHED_DST_PORT 0x2BCB 
#define MEMCACHED_SRC_PORT 0x5555 
#define DUMMY_PORT 0x0000
//#define FAAS_CLIENT_PORT 2222 

parser start {
    return parse_eth;
}

parser parse_eth {
    extract(eth);
    set_metadata(meta.tmpEthAddr, eth.dstAddr);
    return select (eth.etherType) {
        IPV4_TYPE : parse_ipv4;
        default: ingress;
    }
}


parser parse_ipv4 {
    extract(ipv4);
    set_metadata(meta.tmpIpAddr, ipv4.dstAddr);
    set_metadata(meta.tcpLength, ipv4.totalLen - 20);
    return select(ipv4.protocol) {
        UDP_PROTOCOL : parse_udp;
        TCP_PROTOCOL : parse_tcp;
        default : ingress;
    }
}


parser parse_udp {
    extract(udp);
    set_metadata(meta.tmpUdpPort, udp.dstPort);
    return  select(udp.dstPort) {
        SERVER_PORT : parse_payload;
        DUMMY_PORT: parse_memcached;
        default : ingress;
    }
}

parser parse_payload {
    extract(pload);
    return ingress;
}

parser parse_memcached {
    extract(memcached);
    return ingress;
}

parser parse_tcp {
    extract(tcp);
    return  ingress;
}
