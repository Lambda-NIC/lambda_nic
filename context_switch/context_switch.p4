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



//Ingress Control

control ingress {
    if (valid(udp)) {
        if (udp.dstPort == SERVER_PORT) {
            if (valid(pload)) {
                if (pload.jobId == 5) {
                    apply(add_payload1);
                    apply(return_pkt1);
                } else if (pload.jobId == 6) {
                    apply(add_payload2);
                    apply(return_pkt2);
                } else if (pload.jobId == 7) {
                    apply(add_payload3);
                    apply(return_pkt3);
                }
            }
        } else if (udp.dstPort == INTERIM_PORT) {
            apply(out_payload);
            apply(switch_pkt2);
        } else {
            apply(switch_pkt3);
        }
    } else {
        apply(switch_pkt1);
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


// Fuunction to serve simple web request.
action do_serve_request1() {
    serve_request1();
}

action do_serve_request2() {
    serve_request2();
}

action do_serve_request3() {
    serve_request3();
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

table add_payload1 {
    actions {
        do_serve_request1;
    }
}

table add_payload2 {
    actions {
        do_serve_request2;
    }
}

table add_payload3 {
    actions {
        do_serve_request3;
    }
}

table return_pkt1 {
    actions { 
        set_return_hop;
    }
}

table return_pkt2 {
    actions { 
        set_return_hop;
    }
}

table return_pkt3 {
    actions { 
        set_return_hop;
    }
}

table switch_pkt1 {
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
//

// Egress Control
control egress {

}


// /Egress Control
