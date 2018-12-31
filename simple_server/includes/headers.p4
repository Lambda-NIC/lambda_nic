
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

header_type tcp_t {
    fields {
        srcPort : 16;
        dstPort : 16;
        seqNo : 32;
        ackNo : 32;
        dataOffset :4;
        res : 3;
        ecn : 3;
        ctrl : 6;
        window : 16;
        checksum : 16;
        urgentPtr : 16;
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
header tcp_t tcp;
header payload_t pload;

header_type meta_t {
    fields {
        tmpEthAddr: 48;
        tmpIpAddr: 32;
        tmpUdpPort: 16;
        tcpLength : 16;
    }
}

header_type memcached_binary_t {
	fields {
		magic : 8;
		opcode : 8;
		keyLen : 16;
		extraLen : 8;
		dataType : 8;
		status : 16;
		totalBodyLen : 32;
		opaque : 32;
		CAS : 64;
        data: 80;
    }
}

header memcached_binary_t memcached;

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
primitive_action send_cache_set_pkt();
primitive_action grayscale_img();