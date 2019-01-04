#include "includes/headers.p4"
#include "includes/parser.p4"

//Ingress Control

control ingress {
    if (valid(udp)) {
        if (udp.dstPort == SERVER_PORT) {
            if (valid(pload)) {
                if (pload.jobId == 1) {
                    apply(transform_img);
                    apply(return_pkt);
                }
            }
        } else {
            apply(switch_pkt2);
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

table transform_img {
    actions {
        do_grayscale_img;
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

//

// Egress Control
control egress {
}

// /Egress Control
