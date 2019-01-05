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

//Checksum


//Ingress Control

control ingress {
    if (valid(udp)) {
        if (udp.dstPort == SERVER_PORT) {
            if (valid(pload)) {
                if (pload.jobId == 0 or pload.jobId ==1) {
                    apply(add_payload);
                    apply(return_pkt);
                } else if (pload.jobId == 2 or pload.jobId == 3) {
                    apply(mark_cache);
                    apply(switch_pkt3);
                }
            }
        } else if (udp.dstPort == INTERIM_PORT) {
            apply(out_payload);
            apply(switch_pkt2);
        } else if (udp.dstPort == CLONE_GET_PORT or udp.dstPort == CLONE_SET_PORT) {
            apply(send_cache);
        } else if (udp.srcPort == MEMCACHED_PORT) {
            apply(return_memcached_result);
        } else {
            apply(switch_pkt4);
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
// Marking the incoming packets as cache and clone it at egress.
action do_mark_cache_pkt(clone_port) {
    // Change UDP Port
    modify_field(udp.dstPort, clone_port);
    remove_header(pload);
}

// Reroute cloned packets to send cache information.
action do_send_cache_pkt() {
    add_header(memcached);

    send_cache_pkt();

    // Swap Eth Addr to return the request back to host.
    modify_field(eth.dstAddr, eth.srcAddr);
    modify_field(eth.srcAddr, meta.tmpEthAddr);

    // Swap IP Addr
    modify_field(ipv4.dstAddr, ipv4.srcAddr);
    modify_field(ipv4.srcAddr, meta.tmpIpAddr);


    modify_field(udp.dstPort, MEMCACHED_PORT);
    // Remove UDP
    modify_field(udp.checksum, 0);

    // Swap egress Port
    modify_field(standard_metadata.egress_spec,
                 standard_metadata.ingress_port);
}

action do_return_cache() {
    // Swap Eth Addr to return the request back to host.
    modify_field(eth.dstAddr, eth.srcAddr);
    modify_field(eth.srcAddr, meta.tmpEthAddr);

    // Swap IP Addr
    modify_field(ipv4.dstAddr, ipv4.srcAddr);
    modify_field(ipv4.srcAddr, meta.tmpIpAddr);

    // Swap UDP Port
    modify_field(udp.srcPort, SERVER_PORT);

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

table mark_cache {
    reads {
        pload.jobId : exact;
    }
    actions {
        do_mark_cache_pkt;
    }
}

table send_cache {
    actions {
        do_send_cache_pkt;
    }
}

table return_memcached_result {
    actions {
        do_return_cache;
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
