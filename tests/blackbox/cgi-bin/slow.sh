#!/bin/bash
while true;
do
    echo "FROM_STDOUT"
    sleep 1
    >&2 echo "FROM_STDERR"
    sleep 1
done


