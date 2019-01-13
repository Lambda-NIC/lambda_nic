import sys
import timeit
import numpy as np
import PIL
import memcached_udp
import netifaces as ni
import struct
import SocketServer
import threading
import time

from PIL import Image


SERVER_PORT = 10000
IMAGE_ID = 3
if_name = sys.argv[1]
memcached_server_ip = sys.argv[2]
memcached_port = int(sys.argv[3])
DEBUG = False

server_ip = ni.ifaddresses(if_name)[ni.AF_INET][0]['addr']

print "Memcached info (%s:%s)" % (memcached_server_ip, memcached_port)
client = memcached_udp.Client([(memcached_server_ip, memcached_port)], debug=DEBUG)



def transform_image(img_id):
    tic = timeit.default_timer()
    image_path = "./sample_images/img%s.png" % img_id
    im = PIL.Image.open(image_path)
    I = np.asarray(im)
    toc = timeit.default_timer()

    J = np.zeros((256,256))
    for y in range(256):
        for x in range(256):
            J[y][x] = I[y][x][0]/3 + I[y][x][1]/3 + I[y][x][2]/3

    J = J.astype(np.uint8)
    Image.fromarray(J, 'L')
    return toc - tic


class ThreadedUDPRequestHandler(SocketServer.BaseRequestHandler):

    def handle(self):
        data = self.request[0]
        socket = self.request[1]
        current_thread = threading.current_thread()
        if DEBUG:
            print("{}: client: {}, wrote: {}".format(current_thread.name, self.client_address, data))
        d_tup = struct.unpack('>I16s', data)
        job_id = int(d_tup[0])
        res = None
        if job_id == 0:
            res = "hi:        "
        elif job_id == 1:
            res = str(transform_image(IMAGE_ID))
        elif job_id == 2:
            tic = timeit.default_timer()
            res = client.get("hey")
            if not res:
                res = "Not Found"
            toc = timeit.default_timer()
        elif job_id == 3:
            tic = timeit.default_timer()
            res = client.set("hey", "dude")
            if not res:
                res = "STORED"
            toc = timeit.default_timer()

        if res:
            selt = socket.sendto(res, self.client_address)
            if DEBUG:
                print >>sys.stderr, 'sent %s bytes back to %s' % (sent, address)

class ThreadedUDPServer(SocketServer.ThreadingMixIn, SocketServer.UDPServer):
    pass


if __name__ == "__main__":
    server = ThreadedUDPServer((server_ip, SERVER_PORT), ThreadedUDPRequestHandler)

    server_thread = threading.Thread(target=server.serve_forever)
    server_thread.daemon = True

    try:
        server_thread.start()
        print("Server started at {} port {}".format(server_ip, SERVER_PORT))
        while True: time.sleep(100)
    except (KeyboardInterrupt, SystemExit):
        server.shutdown()
        server.server_close()
        exit()
