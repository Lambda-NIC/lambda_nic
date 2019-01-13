import socket
import sys
import timeit
import numpy as np
import PIL
import memcached_udp
import netifaces as ni
import struct
from PIL import Image

SERVER_PORT = 10000
IMAGE_ID = 3
if_name = sys.argv[1]
memcached_server_ip = sys.argv[2]
memcached_port = int(sys.argv[3])

server_ip = ni.ifaddresses(if_name)[ni.AF_INET][0]['addr']

print "Memcached info (%s:%s)" % (memcached_server_ip, memcached_port)
client = memcached_udp.Client([(memcached_server_ip, memcached_port)], debug=True)

image_path = "./sample_images/img%s.png" % IMAGE_ID
im = PIL.Image.open(image_path)
I = np.asarray(im)

# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Bind the socket to the port
server_address = (server_ip, SERVER_PORT)
print >>sys.stderr, 'starting up on %s port %s' % server_address
sock.bind(server_address)

def transform_image():
    tic = timeit.default_timer()

    J = np.zeros((256,256))
    for y in range(256):
        for x in range(256):
            J[y][x] = I[y][x][0]/3 + I[y][x][1]/3 + I[y][x][2]/3

    J = J.astype(np.uint8)
    Image.fromarray(J, 'L')
    toc = timeit.default_timer()
    return toc - tic


while True:
    print >>sys.stderr, '\nwaiting to receive message'
    data, address = sock.recvfrom(4096)

    print >>sys.stderr, 'received %s bytes from %s' % (len(data), address)
    print >>sys.stderr, data
    d_tup = struct.unpack('>I16s', data)
    job_id = int(d_tup[0])
    res = None
    if job_id == 0:
        res = "hi:        "
    elif job_id == 1:
        res = str(transform_image())
    elif job_id == 2:
        tic = timeit.default_timer()
        res = client.get("hey")
        if not res:
            res = "Not Found"
        toc = timeit.default_timer()
    elif job_id == 3:
        tic = timeit.default_timer()
        res = client.set("hey", "dude")
        toc = timeit.default_timer()

    if res:
        sent = sock.sendto(res, address)
        print >>sys.stderr, 'sent %s bytes back to %s' % (sent, address)
