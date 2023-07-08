#!/usr/bin/env bash

set -Eeuo pipefail

cd slix-bootstrap-pkg

rm -r rootfs
mkdir rootfs
if [ -e slix-fs/slix-lock ]; then
    cat slix-fs/slix-lock
    sleep 1
fi
slix mount --mount slix-fs -p slix &
sleep 1
cp -ar slix-fs/etc slix-fs/usr rootfs
(
    cd rootfs/usr
    rm \
       lib/libasan.so \
       lib/libasan.so.8 \
       lib/libasan.so.8.0.0 \
       lib/libgdruntime.so.4.0.0 \
       lib/libgdruntime.so.4 \
       lib/libgdruntime.so \
       lib/libgfortran.so.5.0.0 \
       lib/libgfortran.so.5 \
       lib/libgfortran.so \
       lib/libgomp.so.1.0.0 \
       lib/libgomp.so.1 \
       lib/libgomp.so \
       lib/libgo.so.22.0.0 \
       lib/libgo.so.22 \
       lib/libgo.so \
       lib/libgphobos.so.4.0.0 \
       lib/libgphobos.so.4 \
       lib/libgphobos.so \
       lib/libitm.so.1.0.0 \
       lib/libitm.so.1 \
       lib/libitm.so \
       lib/liblsan.so.0.0.0 \
       lib/liblsan.so.0 \
       lib/liblsan.so \
       lib/libquadmath.so.0.0.0 \
       lib/libquadmath.so.0 \
       lib/libquadmath.so \
       lib/libtsan.so.2.0.0 \
       lib/libtsan.so.2 \
       lib/libtsan.so \
       lib/libubsan.so.1.0.0 \
       lib/libubsan.so.1 \
       lib/libubsan.so
    rm -r lib/{pkgconfig,cmake}
    rm -r share/{man,doc,fish,locale,zsh,info,aclocal}
    rm -r include
)
cat slix-fs/slix-lock
