#include "includes/headers.p4"
#include "includes/parser.p4"


//Checksum

//IPv4 Checksum
field_list ipv4_checksum_list {
    ipv4.version;
    ipv4.ihl;
    ipv4.diffserv;
    ipv4.totalLen;
    ipv4.identification;
    ipv4.flags;
    ipv4.fragOffset;
    ipv4.ttl;
    ipv4.protocol;
    16'0;
    ipv4.srcAddr;
    ipv4.dstAddr;
}

field_list_calculation ipv4_checksum {
    input {
        ipv4_checksum_list;
    }
    algorithm : csum16;
    output_width : 16;
}

calculated_field ipv4.hdrChecksum  {
    verify ipv4_checksum; //Packet received back from controller where NAT is also done gets dropped if verify is enabled. How can we enable this for all other ports? (standard_metadata.ingress_port?)
    update ipv4_checksum; 
}
// /IPv4 Checksum


//TCP Checksum
field_list tcp_ipv4_checksum_list {
    ipv4.srcAddr;
    ipv4.dstAddr;
    8'0;
    ipv4.protocol;
    meta.tcpLength;
    tcp.srcPort;
    tcp.dstPort;
    tcp.seqNo;
    tcp.ackNo;
    tcp.dataOffset;
    tcp.res;
    tcp.ecn;
    tcp.ctrl;
    tcp.window;
    tcp.urgentPtr;
    payload;
}

field_list_calculation tcp_ipv4_checksum {
    input {
        tcp_ipv4_checksum_list;
    }
    algorithm : csum16;
    output_width : 16;
}

calculated_field tcp.checksum  {
    verify tcp_ipv4_checksum if (valid(ipv4));   //Packet received back from controller where NAT is also done gets dropped if verify is enabled. How can we enable this for all other ports? (standard_metadata.ingress_port?)
    update tcp_ipv4_checksum if (valid(ipv4));
}
//TCP Checksum


//Checksum


//Ingress Control

control ingress {
    if (valid(udp)) {
        if (valid(pload)) {
            apply(add_payload);
        }
        apply(switch_pkt);
    }
}

// /Ingress Control

primitive_action serve_request();

action set_nhop() {
    modify_field(ipv4.dstAddr, ipv4.srcAddr);
    modify_field(eth.dstAddr, eth.srcAddr);
    modify_field(ipv4.srcAddr, meta.tmpIpAddr);
    modify_field(eth.srcAddr, meta.tmpEthAddr);
    modify_field(standard_metadata.egress_spec, standard_metadata.ingress_port);
}

action do_serve_request() {
    serve_request();
}

table add_payload {
    actions {
        do_serve_request;
    }
}

table switch_pkt {
    actions { 
        set_nhop; 
    }
}
//


// Egress Control

control egress {

}

// /Egress Control