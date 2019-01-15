import sys
import timeit
import numpy as np
import PIL
import memcached_udp
import netifaces as ni
import struct
import socketserver
import threading
import time
import multiprocessing

from PIL import Image

SERVER_PORT = 10000
IMAGE_ID = 3
if_name = sys.argv[1]
memcached_server_ip = sys.argv[2]
memcached_port = int(sys.argv[3])
DEBUG = False

server_ip = ni.ifaddresses(if_name)[ni.AF_INET][0]['addr']

def simple_response1():
    return "hi:             "

def simple_response2():
    return "bye:            "

def simple_response3():
    return "welcome:        "

class ThreadedUDPRequestHandler(socketserver.BaseRequestHandler):

    def handle(self):
        data = self.request[0]
        socket = self.request[1]
        current_thread = threading.current_thread()
        if DEBUG:
            print("{}: client: {}, wrote: {}".format(current_thread.name, self.client_address, data))
        d_tup = struct.unpack('>I16s', data)
        job_id = int(d_tup[0])
        res = None
        if job_id == 5:
            res = simple_response1()
        elif job_id == 6:
            res = simple_response2()
        elif job_id == 7:
            res = simple_response3()

        if res:
            sent = socket.sendto(res.encode(), self.client_address)
            if DEBUG:
                print('sent %s bytes back to %s' % (sent, self.client_address))

class ThreadedUDPServer(socketserver.ThreadingMixIn, socketserver.UDPServer):
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
