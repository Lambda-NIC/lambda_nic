#include "includes/headers.p4"
#include "includes/parser.p4"

//Ingress Control

control ingress {
    apply(switch_pkt);
}

// /Ingress Control




action set_nhop(port) {
    modify_field(standard_metadata.egress_spec, port);
}

table switch_pkt {
    reads {
        standard_metadata.ingress_port : exact;  //port number - internal and external should be scanned  
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