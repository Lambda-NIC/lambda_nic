#!/bin/bash

tail -n 1800 $1 > /tmp/lambdanic_data.txt
python cleanup_data.py /tmp/lambdanic_data.txt $2 | tail -n 1000 | sort -n
rm /tmp/lambdanic_data.txt
