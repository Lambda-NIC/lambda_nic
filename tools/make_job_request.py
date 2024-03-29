import sys
import requests
import timeit
import threading

DEBUG = False


def make_request(url, params, num_requests):
    num_failures = 0
    for i in range(num_requests):
        try:
            res = requests.post(url, data = params)
        except requests.exceptions.ConnectionError:
            num_failures += 1
            continue
        #print(res.status_code, res.text)
    return num_failures

class myThread(threading.Thread):
    def __init__(self, threadID, name, url, params, num_requests):
        threading.Thread.__init__(self)
        self.threadID = threadID
        self.name = name
        self.url = url
        self.params = params
        self.num_requests = num_requests
        self.debug = DEBUG
    def run(self):
        if self.debug:
            print ("Starting " + self.name)
        num_failures = make_request(self.url, self.params, self.num_requests)
        if self.debug:
            print ("Exiting %s, failures: %d", self.name, num_failures)


#BASE_URL =  "http://172.24.90.32:8001/api/v1/namespaces/openfaas/services/http:gateway:/proxy/function/"
BASE_URL =  "http://localhost:8001/api/v1/namespaces/openfaas/services/http:gateway:/proxy/function/"

job_type = sys.argv[1]
job_id = int(sys.argv[2])
num_requests = int(sys.argv[3])
num_threads = int(sys.argv[4])
if num_threads < 1 or num_threads > 56:
    print("Num threads should be >= 1 and <= 8 ")
    sys.exit(1)

url = None
params = None

if job_type == "lambdanic":
    url = BASE_URL + "lambdanictest"
    params = str(job_id)
elif job_type == "baremetal":
    url = BASE_URL + "baremetaltest"
    params = str(job_id)
elif job_type == "docker":
    if job_id == 0:
        url = BASE_URL + "simpleserver"
    elif job_id == 1:
        url = BASE_URL + "imagetransform"
        # TODO: Change image id later
        params = str(3)
    elif job_id == 2 or job_id == 3 or job_id == 4:
        url = BASE_URL + "simplememcached"
        params = str(job_id)
elif job_type == "dockertcp":
    if job_id == 2 or job_id == 3 or job_id == 4:
        url = BASE_URL + "simplememcachedtcp"
        params = str(job_id)
else:
    print("Invalid job type")
    sys.exit(1)


# Create a list of jobs and then iterate through
# the number of threads appending each thread to
# the job list
jobs = []
for i in range(0, num_threads):
    out_list = list()
    thread = myThread(i, "Thread-%d" % i, url, params, num_requests)
    jobs.append(thread)

print("Created %d threads" % len(jobs))

tic = timeit.default_timer()
# Start the threads (i.e. calculate the random number lists)
for j in jobs:
    j.start()

print("Started %d threads" % len(jobs))

# Ensure all of the threads have finished
for j in jobs:
    j.join()
toc = timeit.default_timer()
print("Total Time: ", toc - tic)
