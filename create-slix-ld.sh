#!/usr/bin/env bash

./build.sh
mkdir -p slix-ld-package/rootfs/usr/{bin,lib}
cp build/bin/slix-ld slix-ld-package/rootfs/usr/bin
cp -a /usr/lib/ld-linux-x86-64.so.2 slix-ld-package/rootfs/usr/lib
./build/bin/slix archive --input slix-ld-package --output slix-ld.gar
