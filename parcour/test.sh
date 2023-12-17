#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
# SPDX-License-Identifier: CC0-1.0

set -Eeuo pipefail
export SLIX_LD_DRYRUN=1
if [ ! -e ../build/bin/slix-ld ]; then
    echo "please build slix-ld first"
    exit 1
fi
cp ../build/bin/slix-ld usr/bin
SLIX_ROOT="$(usr/bin/slix-ld --root)"

mkdir -p tmp

echo "call:  ${SLIX_ROOT}/usr/lib/ld-linux-x86-64.so.2 --argv0 app_01 --library-path ${SLIX_ROOT}/usr/lib ${SLIX_ROOT}/usr/bin/.slix-ld-app_01" > tmp/expected.txt
usr/bin/app_01 > tmp/output.txt
cmp tmp/expected.txt tmp/output.txt


echo "call:  ${SLIX_ROOT}/usr/lib/ld-linux-x86-64.so.2 --argv0 app_01 --library-path ${SLIX_ROOT}/usr/lib ${SLIX_ROOT}/usr/bin/.slix-ld-app_01 --para1 test" > tmp/expected.txt
usr/bin/app_01 --para1 test > tmp/output.txt
cmp tmp/expected.txt tmp/output.txt

echo "call:  ${SLIX_ROOT}/usr/lib/ld-linux-x86-64.so.2 --argv0 app_01 --library-path ${SLIX_ROOT}/usr/lib ${SLIX_ROOT}/usr/bin/.slix-ld-app_01 --para1 test" > tmp/expected.txt
${SLIX_ROOT}/usr/bin/app_01 --para1 test > tmp/output.txt
cmp tmp/expected.txt tmp/output.txt

echo "call:  ${SLIX_ROOT}/usr/lib/ld-linux-x86-64.so.2 --argv0 app_01 --library-path ${SLIX_ROOT}/usr/lib ${SLIX_ROOT}/usr/bin/.slix-ld-app_01 --para1 test" > tmp/expected.txt
PATH=${SLIX_ROOT}/usr/bin app_01 --para1 test > tmp/output.txt
cmp tmp/expected.txt tmp/output.txt

echo "call:  ${SLIX_ROOT}/usr/lib/ld-linux-x86-64.so.2 --argv0 app_02 --library-path ${SLIX_ROOT}/usr/lib ${SLIX_ROOT}/usr/lib/.slix-ld-app_02 --para1 test" > tmp/expected.txt
usr/lib/app_02 --para1 test > tmp/output.txt
cmp tmp/expected.txt tmp/output.txt

echo "call:  ${SLIX_ROOT}/usr/lib/ld-linux-x86-64.so.2 --argv0 app_02 --library-path ${SLIX_ROOT}/usr/lib ${SLIX_ROOT}/usr/lib/.slix-ld-app_02 --para1 test" > tmp/expected.txt
PATH=${SLIX_ROOT}/usr/lib app_02 --para1 test > tmp/output.txt
cmp tmp/expected.txt tmp/output.txt

echo "call:  ${SLIX_ROOT}/usr/lib/ld-linux-x86-64.so.2 --argv0 app_02 --library-path ${SLIX_ROOT}/usr/lib ${SLIX_ROOT}/usr/lib/.slix-ld-app_02 --para1 test" > tmp/expected.txt
PATH=usr/lib app_02 --para1 test > tmp/output.txt
cmp tmp/expected.txt tmp/output.txt

echo "call:  ${SLIX_ROOT}/usr/lib/ld-linux-x86-64.so.2 --argv0 ./g++ --library-path ${SLIX_ROOT}/usr/lib ${SLIX_ROOT}/usr/bin/.slix-ld-g++ --para1 test" > tmp/expected.txt
usr/bin/slix-ld ./g++ /usr/bin/g++ --para1 test > tmp/output.txt
cmp tmp/expected.txt tmp/output.txt

echo "call:  ${SLIX_ROOT}/usr/lib/ld-linux-x86-64.so.2 --argv0 ./bin/g++ --library-path ${SLIX_ROOT}/usr/lib ${SLIX_ROOT}/usr/bin/.slix-ld-g++ --para1 test" > tmp/expected.txt
usr/bin/slix-ld ./bin/g++ /usr/bin/g++ --para1 test > tmp/output.txt
cmp tmp/expected.txt tmp/output.txt


rm usr/bin/slix-ld
rm -rf tmp
echo "success"
