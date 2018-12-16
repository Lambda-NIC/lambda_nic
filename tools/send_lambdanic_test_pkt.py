"""
send_lambdanic_test_pkt.py
Sends a udp packet to the IP and eth of choice.
"""

from scapy.all import *
from time import sleep
from scapy.all import Ether, IP, UDP, Raw
from threading import Thread, Event
import struct

SRC_NUM = int(sys.argv[1])
DST_NUM = int(sys.argv[2])
UDP_PORT = int(sys.argv[3])

IFACE = "vf0_1"
SRC_ETH_ADDR = "00:15:4d:00:%d%d:01" % (SRC_NUM, SRC_NUM)
DST_ETH_ADDR = "00:15:4d:00:%d%d:01" % (DST_NUM, DST_NUM)
SRC_IP = "20.20.2%d.101" % SRC_NUM
DST_IP = "20.20.2%d.101" % DST_NUM

class Sniffer(Thread):
    def  __init__(self, interface=IFACE):
        super().__init__()

        self.daemon = True

        self.socket = None
        self.interface = interface
        self.stop_sniffer = Event()

    def run(self):
        self.socket = conf.L2listen(
            type=ETH_P_ALL,
            iface=self.interface,
            filter="ip"
        )

        sniff(
            opened_socket=self.socket,
            prn=self.print_packet,
            stop_filter=self.should_stop_sniffer
        )

    def join(self, timeout=None):
        self.stop_sniffer.set()
        super().join(timeout)

    def should_stop_sniffer(self, packet):
        return self.stop_sniffer.isSet()

    def print_packet(self, packet):
        ip_layer = packet.getlayer(IP)
        if ip_layer is not None:
            #if ip_layer.src == DST_IP:
            print("[!] New Packet: {src} -> {dst}".format(src=ip_layer.src, dst=ip_layer.dst))

sniffer = Sniffer()

print("[*] Start sniffing...")
sniffer.start()

print(SRC_IP, SRC_ETH_ADDR, DST_IP, DST_ETH_ADDR)

data = b"dude"
ether = Ether(dst=DST_ETH_ADDR)
ip = IP(src=SRC_IP, dst=DST_IP, len= 28 + len(data))
udp = UDP(sport=UDP_PORT, dport=UDP_PORT, len= 8 + len(data))
payload = Raw(load=data)
packet = ether / ip / udp / payload
packet.show()

try:
    while True:
        sleep(1)
        # Send the packet to IFACE
        sendp(packet, iface=IFACE, count=1)

except KeyboardInterrupt:
    print("[*] Stop sniffing")
    sniffer.join(2.0)

    if sniffer.isAlive():
        sniffer.socket.close()

