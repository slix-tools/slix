#!/usr/bin/bash


list=(
    tzdata
    iana-etc
    filesystem
    linux-api-headers
    glibc
    gcc-libs
    ncurses
    readline
    bash
)

for i in ${list[@]}; do
    echo "creating $i"
    ./createPackage.sh $i
done
