# Requires running pip install python-memcached-udp
import memcached_udp

SERVER_IP = '30.30.30.105'
PORT = 11211

client = memcached_udp.Client([(SERVER_IP, PORT)], debug=True)
# DO SOMETHING, i.e.
# client.set("hi", "1")
# client.get("hi")
