
/*
  +-------------------------------------------
    Define parser
  +-------------------------------------------
*/


#define IPV4_TYPE 0x0800
#define UDP_PROTOCOL 0x11

parser start {
    return parse_eth;
}



parser parse_eth {
    extract(eth);
    return select (eth.etherType) {
        IPV4_TYPE : parse_ipv4;
        default: ingress;
    }
}


parser parse_ipv4 {
    extract(ipv4);
    return select(ipv4.protocol) {
        UDP_PROTOCOL : parse_udp;
        default : ingress;
    }
}


parser parse_udp {
    extract(udp);
    return  ingress;
}