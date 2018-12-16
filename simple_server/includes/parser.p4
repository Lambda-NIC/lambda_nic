
/*
  +-------------------------------------------
    Define parser
  +-------------------------------------------
*/


#define IPV4_TYPE 0x0800
#define UDP_PROTOCOL 0x11

#define SERVER_PORT 0x1111


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
    return select(ipv4.protocol) {
        UDP_PROTOCOL : parse_udp;
        default : ingress;
    }
}


parser parse_udp {
    extract(udp);
    return  select(udp.dstPort) {
        SERVER_PORT : parse_payload;
        default : ingress;
    }
}

parser parse_payload {
    extract(pload);
    return ingress;
}
