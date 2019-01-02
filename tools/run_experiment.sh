#!/bin/bash -e

# Arguments
# 1. lambdanic or docker
# 2. job ID
# 3. Number of requests per thread
# 4. Number of threads
# 5. Number of experiments

COUNTER=0
while [  $COUNTER -lt $5 ]; do
    python3 make_job_request.py $1 $2 $3 $4
    let COUNTER-=1
done
