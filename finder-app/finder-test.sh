#!/bin/bash

WRITEDIR=./write-data

make $1

if [ ! -x ./writer ]; then
    echo "Error: writer not found or not executable"
    exit 1
fi

for i in {1..1}; do
    FILENAME="$WRITEDIR/aeld_assignment_2-$i.txt"
    ./writer "$FILENAME" "aeld_assignment-2 file $i"
    rc=$?
    if [ $rc -ne 0 ]; then
        echo "Failed to write to $FILENAME"
        exit 1
    fi
done

echo "wrote successfully"