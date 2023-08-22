#!/usr/bin/env bash

version=0.0.1

./build.sh
mkdir -p slix-ld-package/rootfs/usr/{bin,lib}
mkdir -p slix-ld-package/meta
cp build/bin/slix-ld slix-ld-package/rootfs/usr/bin
cp -a /usr/lib/ld-linux-x86-64.so.2 slix-ld-package/rootfs/usr/lib
echo "" > slix-ld-package/meta/dependencies.txt
echo "slix-ld" > slix-ld-package/meta/name.txt
echo "${version}" > slix-ld-package/meta/version.txt
echo "wrapper around dynamic linking for slix environments" > slix-ld-package/meta/description.txt

./build/bin/slix archive --input slix-ld-package --output slix-ld.gar
