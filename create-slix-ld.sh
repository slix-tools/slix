#!/usr/bin/env bash


./build.sh
mkdir -p slix-ld-package/rootfs/usr/bin
cp build/bin/slix-ld slix-ld-package/rootfs/usr/bin
./build/bin/slix archive --input slix-ld-package --output slix-ld-package.gar

hash=$(sha256sum -b slix-ld-package.gar | awk '{print $1}')
mv slix-ld-package.gar slix-ld@1.0.0-${hash}.gar
