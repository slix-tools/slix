#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
# SPDX-License-Identifier: CC0-1.0


version=0.0.3

BUILD_TYPE=release ./build.sh
mkdir -p slix-ld-package/rootfs/usr/{bin,lib}
mkdir -p slix-ld-package/meta
cp build/bin/slix-ld slix-ld-package/rootfs/usr/bin
cp -a /usr/lib/ld-linux-x86-64.so.2 slix-ld-package/rootfs/usr/lib
echo "" > slix-ld-package/meta/dependencies.txt
echo "slix-ld" > slix-ld-package/meta/name.txt
echo "${version}" > slix-ld-package/meta/version.txt
echo "wrapper around dynamic linking for slix environments" > slix-ld-package/meta/description.txt

./build/bin/slix archive --input slix-ld-package --output slix-ld.gar
hash=$(sha256sum slix-ld.gar | awk '{print $1}')
cp slix-ld.gar slix-ld@${version}\#${hash}.gar
