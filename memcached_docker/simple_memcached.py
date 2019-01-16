import sys
import timeit
import memcached_udp

# IP of the master
SERVER_IP = '10.10.20.105'
PORT = 15000

def get_stdin():
    buf = ""
    for line in sys.stdin:
        buf = buf + line
    buf = buf.strip()
    return buf

if __name__ == "__main__":
    st = get_stdin()
    try:
        client = memcached_udp.Client([(SERVER_IP, PORT)], debug=False)
        jobId = int(st)
        if jobId == 2:
            print(client.get("hey"))
        elif jobId == 3:
            print(client.set("hey", "dude"))
    except Exception as error:
        print("Error: %s" % error)
