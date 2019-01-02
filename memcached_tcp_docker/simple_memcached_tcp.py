import sys
import timeit
from pymemcache.client.base import Client

# IP of the master
SERVER_IP = '10.10.20.105'
PORT = 11211

def get_stdin():
    buf = ""
    for line in sys.stdin:
        buf = buf + line
    buf = buf.strip()
    return buf


if __name__ == "__main__":
    st = get_stdin()
    try:
        tic = timeit.default_timer()
        client = Client((SERVER_IP, PORT))
        toc = timeit.default_timer()
        jobId = int(st)
        if jobId == 2:
            tic = timeit.default_timer()
            res = client.get("hey")
            toc = timeit.default_timer()
            print res, toc - tic
        elif jobId == 3:
            tic = timeit.default_timer()
            res = client.set("hey", "dude")
            toc = timeit.default_timer()
            print res, toc - tic
        elif jobId == 4:
            print toc - tic
    except Exception as error:
        print "Error: %s" % error
