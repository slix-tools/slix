#!/usr/bin/bash

#echo "translate: $1"
if [ -z "$1" ]; then
    exit
fi

Name=$(pacman -Qi $1 \
    | grep -P "^Name" \
    | tr '\n' ' ' \
    | awk '{print $3}')
echo ${Name}
