#!/bin/bash

cat $1 | grep "Total" | cut -d" " -f4
