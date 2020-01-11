#!/bin/bash

cat $1 | grep -v Timeout |  grep '0\.\|e-' |  sort -R | tail -1000
