"""
send_lambdanic_test_pkt.py
Sends a udp packet to the IP and eth of choice.
"""

import struct
import timeit
import sys
import socket
from multiprocessing import Process, Pool


DST_NUM = int(sys.argv[1])
SRC_UDP_PORT = 2222
DST_UDP_PORT = int(sys.argv[2])
NUM_PROCESSES = int(sys.argv[3])
NUM_PACKETS = int(sys.argv[4])
BUFFER_SIZE = 1024

IFACE = "eno1np0"
SRC_ETH_ADDR = "b0:26:28:1a:75:60"
DST_ETH_ADDR = "00:15:4d:00:%d%d:01" % (DST_NUM, DST_NUM)
SRC_IP = "10.10.20.105"
DST_IP = "10.10.10%d.101" % DST_NUM

serverAddressPort   = (DST_IP, DST_UDP_PORT)

# Connect2Server forms the thread - for each connection made to the server
def query_workload(job_id):

    #Create a socket instance - A datagram socket
    UDPClientSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)

    data = struct.pack('>I', job_id)
    data += b"dudedudedudedude"

    # Send message to server using created UDP socket
    UDPClientSocket.sendto(data, serverAddressPort)

    #Receive message from the server
    msgFromServer = UDPClientSocket.recvfrom(BUFFER_SIZE)
    return msgFromServer[0]

if __name__ == "__main__":
    # TODO: Create a list of 5,6,7s
    workloads = [5,6,7] * int(NUM_PACKETS/3)
    tic = timeit.default_timer()
    with Pool(NUM_PROCESSES) as pool:
        pool.map(query_workload, workloads)
    toc = timeit.default_timer()
    print ("Time taken: %f" % (toc - tic))
