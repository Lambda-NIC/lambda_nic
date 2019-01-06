header_type ethernet_t {
    fields {
        dstAddr : 48;
        srcAddr : 48;
        etherType : 16;
    }
}

header_type ipv4_t {
    fields {
        version : 4;
        ihl : 4;
        diffserv : 8;
        totalLen : 16;
        identification : 16;
        flags : 3;
        fragOffset : 13;
        ttl : 8;
        protocol : 8;
        hdrChecksum : 16;
        srcAddr : 32;
        dstAddr: 32;
    }
}

header_type udp_t {
    fields {
        srcPort : 16;
        dstPort : 16;
        length_ : 16;
        checksum : 16;
    }
}

header_type payload_t {
    fields {
        jobId   : 32;
        p       : 128;
    }
}

header ethernet_t eth;
header ipv4_t ipv4;
header udp_t udp;
header payload_t pload;

header_type meta_t {
    fields {
        tmpEthAddr: 48;
        tmpIpAddr: 32;
        tmpUdpPort: 16;
    }
}

header_type memcached_ascii_t {
	fields {
        data: 256;
    }
}

header memcached_ascii_t memcached;

header_type i2e_metadata_t {
    fields {
        ingress_tstamp    : 32;
    }
}

metadata i2e_metadata_t i2e_metadata;

field_list i2e_mirror_info {
    i2e_metadata.ingress_tstamp;
}

metadata meta_t meta;

primitive_action serve_request();
primitive_action send_cache_pkt();