#!/usr/bin/env bash

pkg=$1

if [ -z "${pkg}" ]; then
    exit
fi

#echo $1 > t2.txt
#for i in $(seq 1 100); do
#(while [ $(wc -l t2.txt | cut -f 1 -d ' ') -ge 1 ]; do
mkdir -p deps

echo ${pkg}
if [ ! -e deps/${pkg}-all.txt ]; then
    if [ ! -e deps/${pkg}-direct.txt ]; then
        pacman -Qi ${pkg} \
            | grep "Depends On" \
            | cut -d ':' -f 2 \
            | tr ' ' '\n' \
            | grep -v None \
            | grep . \
            | sort -u \
            | grep -v "\.so" \
            | xargs -n 1 ./translateName2.sh \
            | sort -u \
            > deps/${pkg}-direct.txt


    fi
    (while read pkg; do
        ./listDependencies.sh ${pkg}
    done < deps/${pkg}-direct.txt) | cat -n | tac | sort -uk2 | sort -n | cut -f2- > deps/${pkg}-all.txt
#    while read pkg; do
#        ./listDependencies.sh ${pkg}
#    done < deps/${pkg}-direct.txt

fi
cat deps/${pkg}-all.txt
