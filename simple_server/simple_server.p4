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
    verify ipv4_checksum;
    update ipv4_checksum; 
}
// /IPv4 Checksum


//TCP Checksum
/*
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
// /TCP Checksum

//Similarly, we need to update UDP Checksum. We can't check it, though...
field_list udp_checksum_list {
    ipv4.srcAddr;
    ipv4.dstAddr;
    8'0;
    ipv4.protocol;
    udp.length_;
    udp.srcPort;
    udp.dstPort;
    udp.length_ ;
    payload;
}

field_list_calculation udp_checksum {
    input {
        udp_checksum_list;
    }
    algorithm : csum16;
    output_width : 16;
}

calculated_field udp.checksum {
    update udp_checksum;
}
*/

//Checksum


//Ingress Control

control ingress {
    if (valid(udp)) {
        if (udp.dstPort == SERVER_PORT) {
            if (valid(pload)) {
                if (pload.jobId == 0) {
                    apply(add_payload);
                    apply(return_pkt);
                } else if (pload.jobId == 1) {
                    apply(transform_img);
                    apply(return_pkt2);
                } else if (pload.jobId == 2) {
                    apply(mark_cache_get);
                    apply(switch_pkt3);
                } else if (pload.jobId == 3) {
                    apply(mark_cache_set);
                    apply(switch_pkt4);
                }
            }
        } else if (udp.dstPort == INTERIM_PORT) {
            apply(out_payload);
            apply(switch_pkt2);
        } else if (udp.dstPort == CLONE_GET_PORT) {
            apply(send_cache_get);
        } else if (udp.dstPort == CLONE_SET_PORT) {
            apply(send_cache_set);
        } else {
            apply(switch_pkt5);
        }
    } else {
        apply(switch_pkt);
    }
}

// /Ingress Control

action set_nhop(port) {
    modify_field(standard_metadata.egress_spec, port);
}

action set_return_hop() {
    // Swap Eth Addr
    modify_field(eth.dstAddr, eth.srcAddr);
    modify_field(eth.srcAddr, meta.tmpEthAddr);

    // Swap IP Addr
    modify_field(ipv4.dstAddr, ipv4.srcAddr);
    modify_field(ipv4.srcAddr, meta.tmpIpAddr);

    // Swap UDP Port
    modify_field(udp.dstPort, udp.srcPort);
    modify_field(udp.srcPort, meta.tmpUdpPort);

    // Remove UDP
    modify_field(udp.checksum, 0);

    // Swap egress Port
    modify_field(standard_metadata.egress_spec,
                 standard_metadata.ingress_port);
}


// Actions

// Perform the grayscaler function.
action do_grayscale_img() {
    grayscale_img();
}

// Marking the incoming packets as cache and clone it at egress.
action do_mark_cache_set_pkt() {
    // Change UDP Port
    modify_field(udp.dstPort, CLONE_SET_PORT);
    remove_header(pload);
}

// Reroute cloned packets to send cache information.
action do_send_cache_set_pkt() {
    add_header(memcached);

    send_cache_set_pkt();
    modify_field(ipv4.totalLen, 57);
    modify_field(udp.length_, 37);
    
    // Swap Eth Addr to return the request back to host.
    modify_field(eth.dstAddr, eth.srcAddr);
    modify_field(eth.srcAddr, meta.tmpEthAddr);

    // Swap IP Addr
    modify_field(ipv4.dstAddr, ipv4.srcAddr);
    modify_field(ipv4.srcAddr, meta.tmpIpAddr);

    // Swap UDP Port
    modify_field(udp.dstPort, MEMCACHED_PORT);
    modify_field(udp.srcPort, MEMCACHED_PORT);

    // Remove UDP
    modify_field(udp.checksum, 0);

    // Swap egress Port
    modify_field(standard_metadata.egress_spec,
                 standard_metadata.ingress_port);
}

// Marking the incoming packets as cache and clone it at egress.
action do_mark_cache_get_pkt() {
    // Change UDP Port
    modify_field(udp.dstPort, CLONE_GET_PORT);
    remove_header(pload);
}

// Reroute cloned packets to send cache information.
action do_send_cache_get_pkt() {
    add_header(memcached);

    send_cache_get_pkt();

    modify_field(udp.length_, 25);
    modify_field(ipv4.totalLen, 45);

    // Swap Eth Addr to return the request back to host.
    modify_field(eth.dstAddr, eth.srcAddr);
    modify_field(eth.srcAddr, meta.tmpEthAddr);

    // Swap IP Addr
    modify_field(ipv4.dstAddr, ipv4.srcAddr);
    modify_field(ipv4.srcAddr, meta.tmpIpAddr);

    // Swap UDP Port
    modify_field(udp.dstPort, MEMCACHED_PORT);
    modify_field(udp.srcPort, MEMCACHED_PORT);

    // Remove UDP
    modify_field(udp.checksum, 0);

    // Swap egress Port
    modify_field(standard_metadata.egress_spec,
                 standard_metadata.ingress_port);
}

// Fuunction to serve simple web request.
action do_serve_request() {
    serve_request();
}

action do_out_payload() {
    modify_field(udp.dstPort, SERVER_PORT);
}

// Tables

table out_payload {
    actions {
        do_out_payload;
    }
}

table mark_cache_get {
    actions {
        do_mark_cache_get_pkt;
    }
}

table send_cache_get {
    actions {
        do_send_cache_get_pkt;
    }
}

table mark_cache_set {
    actions {
        do_mark_cache_set_pkt;
    }
}

table send_cache_set {
    actions {
        do_send_cache_set_pkt;
    }
}

table transform_img {
    actions {
        do_grayscale_img;
    }
}

table add_payload {
    actions {
        do_serve_request;
    }
}

table return_pkt {
    actions { 
        set_return_hop;
    }
}

table return_pkt2 {
    actions { 
        set_return_hop;
    }
}

table switch_pkt {
    reads {
        standard_metadata.ingress_port : exact;
    }
    actions {
        set_nhop;
    }
}

table switch_pkt2 {
    reads {
        standard_metadata.ingress_port : exact;
    }
    actions {
        set_nhop;
    }
}

table switch_pkt3 {
    reads {
        standard_metadata.ingress_port : exact;
    }
    actions {
        set_nhop;
    }
}

table switch_pkt4 {
    reads {
        standard_metadata.ingress_port : exact;
    }
    actions {
        set_nhop;
    }
}

table switch_pkt5 {
    reads {
        standard_metadata.ingress_port : exact;
    }
    actions {
        set_nhop;
    }
}

//

// Egress Control
control egress {
    if (valid(udp)) {
        if (udp.dstPort == CLONE_GET_PORT or udp.dstPort == CLONE_SET_PORT) {
            apply(clone_pkt);
        }
    }
}

action do_clone_pkt() {
    // Some session ID of 100.
    clone_egress_pkt_to_ingress(100, i2e_mirror_info);
}


table clone_pkt {
    actions {
        do_clone_pkt;
    }
}
// /Egress Control
