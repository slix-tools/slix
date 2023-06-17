#!/usr/bin/env bash


./build.sh
mkdir -p slix-ld-package/rootfs/usr/bin
cp build/bin/slix-ld slix-ld-package/rootfs/usr/bin
./build/bin/slix archive --input slix-ld-package --output slix-ld.gar
