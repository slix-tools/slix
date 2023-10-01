#!/usr/bin/env bash

set -Eeuo pipefail

mkdir -p build/{obj,bin}
FLAGS="-std=c++20 -D_FILE_OFFSET_BITS=64 -isystem libs/clice/src"

if [ "${BUILD_TYPE:-debug}" == "release" ]; then
    echo "using release settings"
    FLAGS="${FLAGS} -O2 -DNDEBUG -s"
else
    FLAGS="${FLAGS} -ggdb -O0"
fi
cmds="archive env index-add index-init index-info index-push index-squash mount run reset store sync"
objs=""
for cmd in ${cmds}; do
    g++ ${FLAGS} -c src/slix-${cmd}.cpp -o build/obj/slix-${cmd}.cpp.o
    objs="${objs} build/obj/slix-${cmd}.cpp.o"
done
g++ ${FLAGS} -c src/slix.cpp -o build/obj/slix.cpp.o

g++ build/obj/slix.cpp.o \
    ${objs} \
    -lcurl -lfuse -lfmt -lcrypto -lyaml-cpp \
    -o build/bin/slix

ln -fs slix build/bin/slix-env

g++ ${FLAGS} -c src/slix-ld.cpp -o build/obj/slix-ld.cpp.o -static
g++ build/obj/slix-ld.cpp.o -o build/bin/slix-ld -static

if [ "${1:-}" == "install" ] || [ "${1:-}" == "--install" ]; then
    cp -a build/bin/{slix,slix-env} ${HOME}/.local/bin
    echo "installed into ${HOME}/.local/bin"
fi
