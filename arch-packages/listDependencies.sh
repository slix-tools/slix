#!/usr/bin/env bash

echo $1 > t2.txt
#for i in $(seq 1 100); do
#(while [ $(wc -l t2.txt | cut -f 1 -d ' ') -ge 1 ]; do
mkdir -p deps
lastLen=0
while [ $(wc -l t2.txt | cut -f 1 -d ' ') -ne ${lastLen} ]; do
    lastLen=$(wc -l t2.txt | cut -f 1 -d ' ')
    echo -n "" > t1.txt
    while read pkg; do
        if [ ! -e deps/${pkg}.txt ]; then
            pacman -Qi ${pkg} \
                | grep "Depends On" \
                | cut -d ':' -f 2 \
                | tr ' ' '\n' \
                | grep -v None \
                | grep . \
                | sort -u \
                | xargs -n 1 ./translateName2.sh \
                | sort -u \
                > deps/${pkg}.txt
        fi
        cat deps/${pkg}.txt >> t1.txt
    done < t2.txt
    tac t1.txt t2.txt | cat -n | sort -uk2 | sort -n | cut -f2- | tac > t3.txt
    mv t3.txt t2.txt
done
cat t2.txt
rm t1.txt t2.txt
