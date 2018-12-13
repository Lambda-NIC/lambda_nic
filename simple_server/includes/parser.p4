
/*
  +-------------------------------------------
    Define parser
  +-------------------------------------------
*/


#define IPV4_TYPE 0x0800
#define UDP_PROTOCOL 0x11
#define TCP_PROTOCOL 0x06

#define SERVER_PORT 0x1111


parser start {
    return parse_eth;
}



parser parse_eth {
    extract(eth);
    set_metadata(meta.tmpEthAddr, eth.srcAddr);
    return select (eth.etherType) {
        IPV4_TYPE : parse_ipv4;
        default: ingress;
    }
}


parser parse_ipv4 {
    extract(ipv4);
    set_metadata(meta.tmpIpAddr, ipv4.srcAddr);
    set_metadata(meta.tcpLength, ipv4.totalLen - 20);
    return select(ipv4.protocol) {
        TCP_PROTOCOL : parse_tcp;
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


parser parse_tcp {
    extract(tcp);
    return  ingress;
}

