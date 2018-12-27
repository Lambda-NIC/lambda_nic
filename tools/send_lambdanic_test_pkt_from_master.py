"""
send_lambdanic_test_pkt.py
Sends a udp packet to the IP and eth of choice.
"""

from scapy.all import *
from time import sleep
from scapy.all import Ether, IP, UDP, Raw
from threading import Thread, Event
import struct
from datetime import datetime

DST_NUM = int(sys.argv[1])
SRC_UDP_PORT = 2222
DST_UDP_PORT = int(sys.argv[2])
JOB_ID = int(sys.argv[3])

IFACE = "eno1np0"
SRC_ETH_ADDR = "b0:26:28:1a:75:60"
DST_ETH_ADDR = "00:15:4d:00:%d%d:01" % (DST_NUM, DST_NUM)
SRC_IP = "30.30.30.105"
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
            filter="udp"
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
            print("[!] New Packet: {src} -> {dst} time: {time}".format(src=ip_layer.src, dst=ip_layer.dst, time=ip_layer.time))

sniffer = Sniffer()

print("[*] Start sniffing...")
sniffer.start()

print(SRC_IP, SRC_ETH_ADDR, DST_IP, DST_ETH_ADDR)

data = struct.pack('>I', JOB_ID)
data += b"dudedudedudedude"
ether = Ether(dst=DST_ETH_ADDR)
ip = IP(src=SRC_IP, dst=DST_IP, len= 28 + len(data))
udp = UDP(sport=SRC_UDP_PORT, dport=DST_UDP_PORT, len= 8 + len(data))
payload = Raw(load=data)
packet = ether / ip / udp / payload
packet.show()

try:
    while True:
        sleep(1)
        sendp(packet, iface=IFACE, count=1)

except KeyboardInterrupt:
    print("[*] Stop sniffing")
    sniffer.join(2.0)

    if sniffer.isAlive():
        sniffer.socket.close()

