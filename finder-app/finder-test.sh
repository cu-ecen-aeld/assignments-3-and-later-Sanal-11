#!/bin/bash

make $1

if [ ! -x ./writer ]; then
    echo "Error: writer not found or not executable"
    exit 1
fi

./writer testfile.txt "testing word"
rc=$?

if [ $rc -ne 0 ]; then
    exit 1
fi