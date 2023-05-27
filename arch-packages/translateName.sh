#!/usr/bin/bash

if [ -z "$1" ]; then
    exit
fi

Name=$(pacman -Qi $1 \
    | grep -P "^Name" \
    | tr '\n' ' ' \
    | awk '{print $3}')
Version=$(pacman -Qi $1 \
    | grep -P "^Version" \
    | tr '\n' ' ' \
    | awk '{print $3}')
FileName=$(ls | grep "${Name}@${Version}-")
if [ -z "${FileName}" ]; then
    echo "${Name}@${Version}-Missing.gar"
else
    echo $FileName
fi
