import socket
import sys
import timeit
import numpy as np
import PIL
from PIL import Image

client = memcached_udp.Client([(SERVER_IP, PORT)], debug=False)

# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Bind the socket to the port
server_address = ('localhost', 10000)
print >>sys.stderr, 'starting up on %s port %s' % server_address
sock.bind(server_address)

def transform_image(img_id):
    tic = timeit.default_timer()
    image_path = "./sample_images/img%s.png" % img_id
    im = PIL.Image.open(image_path)
    toc = timeit.default_timer()
    I = np.asarray(im)
    J = np.zeros((256,256))
    for y in range(256):
        for x in range(256):
            J[y][x] = I[y][x][0]/3 + I[y][x][1]/3 + I[y][x][2]/3

    J = J.astype(np.uint8)
    Image.fromarray(J, 'L')
    return toc - tic


while True:
    print >>sys.stderr, '\nwaiting to receive message'
    data, address = sock.recvfrom(4096)

    print >>sys.stderr, 'received %s bytes from %s' % (len(data), address)
    print >>sys.stderr, data
    data = data.strip()
    job_id = int(data)
    res = None
    if job_id == 0:
        res = "hi:        "
    elif job_id == 1:
        res = transform_image(3)
        jobId = int(st)
    elif jobId == 2:
        tic = timeit.default_timer()
        res = client.get("hey")
        toc = timeit.default_timer()
    elif jobId == 3:
        tic = timeit.default_timer()
        res = client.set("hey", "dude")
        toc = timeit.default_timer()

    if res:
        sent = sock.sendto(res, address)
        print >>sys.stderr, 'sent %s bytes back to %s' % (sent, address)
